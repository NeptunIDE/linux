/*
 * linux/include/ve_nfs.h
 *
 * VE context for NFS
 *
 * Copyright (C) 2007 SWsoft
 */

#ifndef __VE_NFS_H__
#define __VE_NFS_H__

struct ve_rpc_data {
	struct proc_dir_entry	*_proc_net_rpc;
	struct cache_detail	*_ip_map_cache;
	struct file_system_type	*rpc_pipefs_fstype;
	struct rpc_clnt		*_rpcb_local;
	struct rpc_clnt		*_rpcb_local4;
	spinlock_t		_rpcb_clnt_lock;
	int			_rpcb_users;
	struct workqueue_struct *_rpciod_workqueue;
	atomic_t		_users;
};

#ifdef CONFIG_NFS_V4
#include <linux/nfs4.h>

#define nfs_callback_tcpport	NFS_CTX_FIELD(nfs_callback_tcpport)
#define nfs_callback_tcpport6	NFS_CTX_FIELD(nfs_callback_tcpport6)

struct nfs_callback_data {
	unsigned int users;
	struct svc_serv *serv;
	struct svc_rqst *rqst;
	struct task_struct *task;
};
#endif

struct ve_nfs_data {
	struct workqueue_struct *_nfsiod_workqueue;
	atomic_t		_users;
#ifdef CONFIG_NFS_V4
	struct nfs_callback_data _nfs_callback_info[NFS4_MAX_MINOR_VERSION + 1];
	struct mutex		_nfs_callback_mutex;

	unsigned short		_nfs_callback_tcpport;
	unsigned short		_nfs_callback_tcpport6;
#endif
};

struct ve_nlm_data {
	unsigned int		_nlmsvc_users;
	struct task_struct*	_nlmsvc_task;
	unsigned long		_nlmsvc_timeout;
	struct svc_rqst*	_nlmsvc_rqst;

	struct hlist_head	_nlm_reserved_pids;
	spinlock_t		_nlm_reserved_lock;

	struct delayed_work	_grace_period_end;
	struct list_head	_grace_list;
	struct lock_manager	_lockd_manager;
};

#ifdef CONFIG_VE

#include <linux/ve.h>

#define NLM_CTX_FIELD(arg)	(get_exec_env()->nlm_data->_##arg)
#define NFS_CTX_FIELD(arg)	(get_exec_env()->nfs_data->_##arg)


static void destroy_rpc_data(struct work_struct *work);

static inline void ve_rpc_data_init(void)
{
	atomic_set(&get_exec_env()->rpc_data->_users, 1);
	spin_lock_init(&get_exec_env()->rpc_data->_rpcb_clnt_lock);
	INIT_WORK(&get_exec_env()->rpc_destroy_work, destroy_rpc_data);
}

static inline void ve_rpc_data_get(void)
{
	atomic_inc(&get_exec_env()->rpc_data->_users);
}

extern void rpcb_put_local(void);
extern void rpciod_stop(void);

static void destroy_rpc_data(struct work_struct *work)
{
	struct ve_struct *ve = container_of(work, struct ve_struct, rpc_destroy_work);

	BUG_ON(!ve_is_super(get_exec_env()));

	set_exec_env(ve);

	rpciod_stop();
	kfree(ve->rpc_data);
	ve->rpc_data = NULL;

	set_exec_env(&ve0);
}

static inline void ve_rpc_data_put(struct ve_struct *ve)
{
	if (atomic_dec_and_test(&ve->rpc_data->_users)) {
		/*
		 * RPC data usage counter have reached zero, and we
		 * have to stop rpciod queue and release virtualized
		 * data. But why we release this data in async queue?
		 * Becuase we can come here from rpciod workqueue:
		 * rpc_async_schedule -> __rpc_execute ->
		 * rpc_release_task -> rpc_final_put_task ->
		 * rpc_free_task -> rpc_release_calldata ->
		 * rpcb_map_release -> xprt_put -> xprt_destroy ->
		 * xs_destroy -> xprt_free -> ve_rpc_data_put ->
		 * rpciod_stop
		 * The only simple solution here is to schedule the same task
		 * in another workqueue.
		 */
		queue_work(ve0.khelper_wq, &ve->rpc_destroy_work);
	}
}

static inline void ve_nfs_data_init(struct ve_nfs_data *data)
{
	atomic_set(&data->_users, 1);
#ifdef CONFIG_NFS_V4
	mutex_init(&data->_nfs_callback_mutex);
#endif
	get_exec_env()->nfs_data = data;
}

static inline void ve_nfs_data_get(void)
{
	atomic_inc(&get_exec_env()->nfs_data->_users);
}

extern void nfsiod_stop(void);
extern inline void ve_nfs_data_put(struct ve_struct *ve);

#else /* CONFIG_VE */

#define NLM_CTX_FIELD(arg)	_##arg
#define NFS_CTX_FIELD(arg)	_##arg

static void ve_rpc_data_init(void)
{}
static void ve_rpc_data_get(void)
{}
static void ve_rpc_data_put(struct ve_struct *ve)
{}

static void ve_nfs_data_init(void)
{}
static void ve_nfs_data_get(void)
{}
static void ve_nfs_data_put(struct ve_struct *ve)
{}

#endif /* CONFIG_VE */

#define nlmsvc_grace_period	NLM_CTX_FIELD(nlmsvc_grace_period)
#define nlmsvc_timeout		NLM_CTX_FIELD(nlmsvc_timeout)
#define nlmsvc_users		NLM_CTX_FIELD(nlmsvc_users)
#define nlmsvc_task		NLM_CTX_FIELD(nlmsvc_task)
#define nlmsvc_rqst		NLM_CTX_FIELD(nlmsvc_rqst)

#define nlm_reserved_pids	NLM_CTX_FIELD(nlm_reserved_pids)
#define nlm_reserved_lock	NLM_CTX_FIELD(nlm_reserved_lock)

#define grace_period_end	NLM_CTX_FIELD(grace_period_end)
#define grace_list		NLM_CTX_FIELD(grace_list)
#define lockd_manager		NLM_CTX_FIELD(lockd_manager)

#define nfsiod_workqueue	NFS_CTX_FIELD(nfsiod_workqueue)

#include <linux/nfsd/stats.h>

#define VE_RAPARM_SIZE	2048

struct ve_nfsd_data {
	struct file_system_type *nfsd_fs;
	struct cache_detail *exp_cache;
	struct cache_detail *key_cache;
	struct svc_serv *_nfsd_serv;
	struct nfsd_stats stats;
	struct svc_stat *svc_stat;
	char raparm_mem[VE_RAPARM_SIZE];
	struct completion exited;
	bool nfsd_up;
};

extern int ve_nfs_sync(struct ve_struct *env, int wait);
extern void nfs_change_server_params(void *data, int flags, int timeo, int retrans);
extern int is_nfs_automount(struct vfsmount *mnt);
#endif
