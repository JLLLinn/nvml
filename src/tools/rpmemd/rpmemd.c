/*
 * Copyright 2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * rpmemd.c -- rpmemd main source file
 */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "libpmem.h"
#include "librpmem.h"
#include "rpmemd.h"
#include "rpmemd_log.h"
#include "rpmemd_config.h"
#include "rpmem_common.h"
#include "rpmemd_fip.h"
#include "rpmemd_obc.h"
#include "rpmemd_db.h"
#include "pool_hdr.h"
#include "util.h"
#include "uuid.h"

/*
 * rpmemd -- rpmem handle
 */
struct rpmemd {
	struct rpmemd_obc *obc;	/* out-of-band connection handle */
	struct rpmemd_db *db;	/* pool set database handle */
	struct rpmemd_db_pool *pool; /* pool handle */
	struct rpmemd_fip *fip;	/* fabric provider handle */
	struct rpmemd_config config; /* configuration */
	size_t nthreads;	/* number of processing threads */
	enum rpmem_persist_method persist_method;
	void (*persist)(const void *addr, size_t len); /* used for GPSPM */
	int closing;		/* set when closing connection */
};

#ifdef DEBUG
/*
 * bool2str -- convert bool to yes/no string
 */
static inline const char *
bool2str(int v)
{
	return v ? "yes" : "no";
}
#endif

/*
 * str_or_null -- return null string instead of NULL pointer
 */
static inline const char *
_str(const char *str)
{
	if (!str)
		return "(null)";
	return str;
}

/*
 * uuid2str -- convert uuid to string
 */
static const char *
uuid2str(const uuid_t uuid)
{
	static char uuid_str[64] = {0, };

	int ret = util_uuid_to_string(uuid, uuid_str);
	if (ret != 0) {
		return "(error)";
	}

	return uuid_str;
}

/*
 * rpmemd_get_nthreads -- returns number of threads to use for fabric
 * processing
 */
static size_t
rpmemd_get_nthreads(void)
{
	long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (ncpus < 0) {
		RPMEMD_LOG(ERR, "getting number of CPUs");
		return 0;
	}

	return (size_t)ncpus;
}

/*
 * rpmemd_get_pm -- returns persist method based on configuration
 */
static enum rpmem_persist_method
rpmemd_get_pm(struct rpmemd_config *config)
{
	enum rpmem_persist_method ret = RPMEM_PM_GPSPM;

	if (config->persist_apm)
		ret = RPMEM_PM_APM;

	return ret;
}

/*
 * rpmemd_db_get_status -- convert error number to status for db operation
 */
static int
rpmemd_db_get_status(int err)
{
	switch (err) {
	case EEXIST:
		return RPMEM_ERR_EXISTS;
	case EACCES:
		return RPMEM_ERR_NOACCESS;
	case ENOENT:
		return RPMEM_ERR_NOEXIST;
	case EWOULDBLOCK:
		return RPMEM_ERR_BUSY;
	default:
		return RPMEM_ERR_FATAL;
	}
}

/*
 * rpmemd_check_pool -- verify pool parameters
 */
static int
rpmemd_check_pool(struct rpmemd *rpmemd, const struct rpmem_req_attr *req,
	int *status)
{
	if (rpmemd->pool->pool_size - POOL_HDR_SIZE < req->pool_size) {
		RPMEMD_LOG(ERR, "requested size is too big");
		*status = RPMEM_ERR_BADSIZE;
		return -1;
	}

	return 0;
}

/*
 * rpmemd_common_fip_init -- initialize fabric provider
 */
static int
rpmemd_common_fip_init(struct rpmemd *rpmemd, const struct rpmem_req_attr *req,
	struct rpmem_resp_attr *resp, int *status)
{
	void *addr = (void *)((uintptr_t)rpmemd->pool->pool_addr +
			POOL_HDR_SIZE);
	struct rpmemd_fip_attr fip_attr = {
		.addr		= addr,
		.size		= req->pool_size,
		.nlanes		= req->nlanes,
		.nthreads	= rpmemd->nthreads,
		.provider	= req->provider,
		.persist_method = rpmemd->persist_method,
		.persist	= rpmemd->persist,
	};

	const char *node = rpmem_get_ssh_conn_addr();
	enum rpmem_err err;

	rpmemd->fip = rpmemd_fip_init(node, NULL, &fip_attr, resp, &err);
	if (!rpmemd->fip) {
		*status = (int)err;
		goto err_fip_init;
	}

	return 0;
err_fip_init:
	return -1;
}

/*
 * rpmemd_print_req_attr -- print request attributes
 */
static void
rpmemd_print_req_attr(const struct rpmem_req_attr *req)
{
	RPMEMD_LOG(NOTICE, "\tpool descriptor: '%s'", _str(req->pool_desc));
	RPMEMD_LOG(NOTICE, "\tpool size: %lu", req->pool_size);
	RPMEMD_LOG(NOTICE, "\tnlanes: %u", req->nlanes);
	RPMEMD_LOG(NOTICE, "\tprovider: %s",
			rpmem_provider_to_str(req->provider));
}

/*
 * rpmemd_print_pool_attr -- print pool attributes
 */
static void
rpmemd_print_pool_attr(const struct rpmem_pool_attr *attr)
{
	RPMEMD_LOG(INFO, "\tsignature: '%s'", _str(attr->signature));
	RPMEMD_LOG(INFO, "\tmajor: %u", attr->major);
	RPMEMD_LOG(INFO, "\tcompat_features: 0x%x", attr->compat_features);
	RPMEMD_LOG(INFO, "\tincompat_features: 0x%x", attr->incompat_features);
	RPMEMD_LOG(INFO, "\tro_compat_features: 0x%x",
			attr->ro_compat_features);
	RPMEMD_LOG(INFO, "\tpoolset_uuid: %s", uuid2str(attr->poolset_uuid));
	RPMEMD_LOG(INFO, "\tuuid: %s", uuid2str(attr->uuid));
	RPMEMD_LOG(INFO, "\tnext_uuid: %s", uuid2str(attr->next_uuid));
	RPMEMD_LOG(INFO, "\tprev_uuid: %s", uuid2str(attr->prev_uuid));
}

static void
rpmemd_print_resp_attr(const struct rpmem_resp_attr *attr)
{
	RPMEMD_LOG(NOTICE, "\tport: %u", attr->port);
	RPMEMD_LOG(NOTICE, "\trkey: 0x%lx", attr->rkey);
	RPMEMD_LOG(NOTICE, "\traddr: 0x%lx", attr->raddr);
	RPMEMD_LOG(NOTICE, "\tnlanes: %u", attr->nlanes);
	RPMEMD_LOG(NOTICE, "\tpersist method: %s",
			rpmem_persist_method_to_str(attr->persist_method));
}

/*
 * rpmemd_req_create -- handle create request
 */
static int
rpmemd_req_create(struct rpmemd_obc *obc, void *arg,
	const struct rpmem_req_attr *req,
	const struct rpmem_pool_attr *pool_attr)
{
	RPMEMD_ASSERT(arg != NULL);
	RPMEMD_LOG(NOTICE, "create request:");
	rpmemd_print_req_attr(req);
	RPMEMD_LOG(NOTICE, "pool attributes:");
	rpmemd_print_pool_attr(pool_attr);

	struct rpmemd *rpmemd = (struct rpmemd *)arg;

	int ret;
	int status = 0;
	int err_send = 1;
	struct rpmem_resp_attr resp;
	memset(&resp, 0, sizeof(resp));

	if (rpmemd->pool) {
		RPMEMD_LOG(ERR, "pool already opened");
		status = RPMEM_ERR_FATAL;
		goto err_pool_opened;
	}

	rpmemd->pool = rpmemd_db_pool_create(rpmemd->db,
			req->pool_desc,
			0, (struct rpmem_pool_attr *)pool_attr);
	if (!rpmemd->pool) {
		status = rpmemd_db_get_status(errno);
		goto err_pool_create;
	}

	ret = rpmemd_check_pool(rpmemd, req, &status);
	if (ret)
		goto err_pool_check;

	ret = rpmemd_common_fip_init(rpmemd, req, &resp, &status);
	if (ret)
		goto err_fip_init;

	RPMEMD_LOG(NOTICE, "create request response: (status = %u)", status);
	if (!status)
		rpmemd_print_resp_attr(&resp);
	ret = rpmemd_obc_create_resp(rpmemd->obc, status, &resp);
	if (ret) {
		err_send = 0;
		goto err_create_resp;
	}

	RPMEMD_LOG(INFO, "waiting for in-band connection");

	ret = rpmemd_fip_accept(rpmemd->fip);
	if (ret) {
		status = RPMEM_ERR_FATAL_CONN;
		goto err_accept;
	}

	RPMEMD_LOG(NOTICE, "in-band connection established");

	ret = rpmemd_fip_process_start(rpmemd->fip);
	if (ret) {
		status = RPMEM_ERR_FATAL_CONN;
		goto err_process_start;
	}

	return 0;
err_process_start:
	rpmemd_fip_close(rpmemd->fip);
err_accept:
	err_send = 0;
err_create_resp:
	rpmemd_fip_fini(rpmemd->fip);
err_fip_init:
err_pool_check:
	rpmemd_db_pool_close(rpmemd->db, rpmemd->pool);
	rpmemd_db_pool_remove(rpmemd->db, req->pool_desc);
err_pool_create:
err_pool_opened:
	if (err_send)
		ret = rpmemd_obc_create_resp(rpmemd->obc, status, &resp);
	rpmemd->closing = 1;
	return ret;
}

/*
 * rpmemd_req_open -- handle open request
 */
static int
rpmemd_req_open(struct rpmemd_obc *obc, void *arg,
	const struct rpmem_req_attr *req)
{
	RPMEMD_ASSERT(arg != NULL);
	RPMEMD_LOG(NOTICE, "open request:");
	rpmemd_print_req_attr(req);

	struct rpmemd *rpmemd = (struct rpmemd *)arg;

	int ret;
	int status = 0;
	int err_send = 1;
	struct rpmem_resp_attr resp;
	memset(&resp, 0, sizeof(resp));

	struct rpmem_pool_attr pool_attr;
	memset(&pool_attr, 0, sizeof(pool_attr));

	if (rpmemd->pool) {
		RPMEMD_LOG(ERR, "pool already opened");
		status = RPMEM_ERR_FATAL;
		goto err_pool_opened;
	}

	rpmemd->pool = rpmemd_db_pool_open(rpmemd->db,
			req->pool_desc, 0, &pool_attr);
	if (!rpmemd->pool) {
		status = rpmemd_db_get_status(errno);
		goto err_pool_open;
	}

	RPMEMD_LOG(NOTICE, "pool attributes:");
	rpmemd_print_pool_attr(&pool_attr);

	ret = rpmemd_check_pool(rpmemd, req, &status);
	if (ret)
		goto err_pool_check;

	ret = rpmemd_common_fip_init(rpmemd, req, &resp, &status);
	if (ret)
		goto err_fip_init;

	RPMEMD_LOG(NOTICE, "open request response: (status = %u)", status);
	if (!status)
		rpmemd_print_resp_attr(&resp);

	ret = rpmemd_obc_open_resp(rpmemd->obc, status, &resp, &pool_attr);
	if (ret) {
		err_send = 0;
		goto err_open_resp;
	}

	RPMEMD_LOG(INFO, "waiting for in-band connection");

	ret = rpmemd_fip_accept(rpmemd->fip);
	if (ret) {
		status = RPMEM_ERR_FATAL_CONN;
		goto err_accept;
	}

	RPMEMD_LOG(NOTICE, "in-band connection established");

	ret = rpmemd_fip_process_start(rpmemd->fip);
	if (ret) {
		status = RPMEM_ERR_FATAL_CONN;
		goto err_process_start;
	}

	return 0;
err_process_start:
	rpmemd_fip_close(rpmemd->fip);
err_accept:
	err_send = 0;
err_open_resp:
	rpmemd_fip_fini(rpmemd->fip);
err_fip_init:
err_pool_check:
	rpmemd_db_pool_close(rpmemd->db, rpmemd->pool);
err_pool_open:
err_pool_opened:
	if (err_send)
		ret = rpmemd_obc_open_resp(rpmemd->obc, status,
				&resp, &pool_attr);
	rpmemd->closing = 1;
	return ret;
}

/*
 * rpmemd_req_close -- handle close request
 */
static int
rpmemd_req_close(struct rpmemd_obc *obc, void *arg)
{
	RPMEMD_ASSERT(arg != NULL);
	RPMEMD_LOG(NOTICE, "close request");

	struct rpmemd *rpmemd = (struct rpmemd *)arg;

	rpmemd->closing = 1;

	int ret;
	int status = 0;

	if (!rpmemd->pool) {
		RPMEMD_LOG(ERR, "pool not opened");
		status = RPMEM_ERR_FATAL;
		return rpmemd_obc_close_resp(rpmemd->obc, status);
	}

	rpmemd_db_pool_close(rpmemd->db, rpmemd->pool);

	ret = rpmemd_fip_process_stop(rpmemd->fip);
	if (ret) {
		RPMEMD_LOG(ERR, "!stopping fip process failed");
		status = errno;
	}

	RPMEMD_LOG(NOTICE, "close request response (status = %u)", status);
	ret = rpmemd_obc_close_resp(rpmemd->obc, status);
	if (!ret)
		rpmemd_fip_wait_close(rpmemd->fip, -1);

	rpmemd_fip_close(rpmemd->fip);
	rpmemd_fip_fini(rpmemd->fip);

	return ret;
}

static struct rpmemd_obc_requests rpmemd_req = {
	.create	= rpmemd_req_create,
	.open	= rpmemd_req_open,
	.close	= rpmemd_req_close,
};

/*
 * rpmemd_print_info -- print basic info and configuration
 */
static void
rpmemd_print_info(struct rpmemd *rpmemd)
{
	RPMEMD_LOG(NOTICE, "ssh connection: %s",
			_str(getenv("SSH_CONNECTION")));
	RPMEMD_LOG(NOTICE, "user: %s", _str(getenv("USER")));
	RPMEMD_LOG(NOTICE, "configuration");
	RPMEMD_LOG(NOTICE, "\tpool set directory: '%s'",
			_str(rpmemd->config.poolset_dir));
	RPMEMD_LOG(NOTICE, "\tpersist method: %s",
			rpmem_persist_method_to_str(rpmemd->persist_method));
	RPMEMD_LOG(NOTICE, "\tnumber of threads: %lu", rpmemd->nthreads);
	RPMEMD_DBG("\tpersist APM: %s",
		bool2str(rpmemd->config.persist_apm));
	RPMEMD_DBG("\tpersist GPSPM: %s",
		bool2str(rpmemd->config.persist_general));
	RPMEMD_DBG("\tuse syslog: %s", bool2str(rpmemd->config.use_syslog));
	RPMEMD_DBG("\tlog file: %s", _str(rpmemd->config.log_file));
	RPMEMD_DBG("\tlog level: %s",
			rpmemd_log_level_to_str(rpmemd->config.log_level));
}

int
main(int argc, char *argv[])
{
	util_init();

	struct rpmemd *rpmemd = calloc(1, sizeof(*rpmemd));
	if (!rpmemd) {
		RPMEMD_LOG(ERR, "!calloc");
		goto err_rpmemd;
	}

	rpmemd->obc = rpmemd_obc_init(STDIN_FILENO, STDOUT_FILENO);
	if (!rpmemd->obc) {
		RPMEMD_LOG(ERR, "out-of-band connection intitialization");
		goto err_obc;
	}

	if (rpmemd_log_init(DAEMON_NAME, NULL, 0)) {
		RPMEMD_LOG(ERR, "logging subsystem initialization failed");
		goto err_log_init;
	}

	if (rpmemd_config_read(&rpmemd->config, argc, argv) != 0) {
		RPMEMD_LOG(ERR, "reading configuration failed");
		goto err_config;
	}

	rpmemd_log_level = rpmemd->config.log_level;
	if (rpmemd_log_init(DAEMON_NAME, rpmemd->config.log_file,
				rpmemd->config.use_syslog)) {
		RPMEMD_LOG(ERR, "logging subsystem initialization"
			" failed (%s, %d)", rpmemd->config.log_file,
			rpmemd->config.use_syslog);
		goto err_log_init_config;
	}

	RPMEMD_LOG(INFO, "%s version %s", DAEMON_NAME, SRCVERSION);
	rpmemd->persist = pmem_persist;
	rpmemd->persist_method = rpmemd_get_pm(&rpmemd->config);
	rpmemd->nthreads = rpmemd_get_nthreads();
	if (!rpmemd->nthreads) {
		RPMEMD_LOG(ERR, "invalid number of threads -- '%lu'",
				rpmemd->nthreads);
		goto err_nthreads;
	}

	rpmemd->db = rpmemd_db_init(rpmemd->config.poolset_dir, 0666);
	if (!rpmemd->db) {
		RPMEMD_LOG(ERR, "!pool set db initialization");
		goto err_db_init;
	}

	int ret = rpmemd_obc_status(rpmemd->obc, 0);
	if (ret) {
		RPMEMD_LOG(ERR, "writing status failed");
		goto err_status;
	}

	rpmemd_print_info(rpmemd);

	while (!ret) {
		ret = rpmemd_obc_process(rpmemd->obc, &rpmemd_req, rpmemd);
		if (ret) {
			RPMEMD_LOG(ERR, "out-of-band connection"
					" process failed");
			goto err;
		}

		if (rpmemd->closing)
			break;
	}

	rpmemd_obc_fini(rpmemd->obc);
	rpmemd_db_fini(rpmemd->db);
	free(rpmemd);
	rpmemd_log_close();
	rpmemd_config_free(&rpmemd->config);

	return 0;
err:
err_status:
	rpmemd_db_fini(rpmemd->db);
err_db_init:
err_nthreads:
err_log_init_config:
	rpmemd_config_free(&rpmemd->config);
err_config:
	rpmemd_log_close();
err_log_init:
	if (rpmemd_obc_status(rpmemd->obc, (uint32_t)errno))
		RPMEMD_LOG(ERR, "writing status failed");
	rpmemd_obc_fini(rpmemd->obc);
err_obc:
	free(rpmemd);
err_rpmemd:
	return 1;
}
