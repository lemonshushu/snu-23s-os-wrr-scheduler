/*
 * Weighted Round-Robin Scheduling Class (mapped to the SCHED_WRR policy)
 */
#include "sched.h"

static inline int move_entity(unsigned int flags) {
	if ((flags & (DEQUEUE_SAVE | DEQUEUE_MOVE)) == DEQUEUE_SAVE)
		return 0;

	return 1;
}

/// @brief Initialize a WRR runqueue.
/// @param wrr_rq WRR runqueue to initiate.
void init_wrr_rq(struct wrr_rq *wrr_rq) {
	INIT_LIST_HEAD(&wrr_rq->queue);
	wrr_rq->bit = 0;
	wrr_rq->nr_running = 0;
}

/// @brief Get the task_struct of a WRR scheduler entity.
static inline struct task_struct *wrr_task_of(struct sched_wrr_entity *wrr_se) {
	return container_of(wrr_se, struct task_struct, wrr);
}

/// @brief Get the runqueue of a WRR runqueue.
static inline struct rq *rq_of_wrr_rq(struct wrr_rq *wrr_rq) {
	return container_of(wrr_rq, struct rq, wrr);
}

/// @brief Get the runqueue of a WRR scheduler entity.
static inline struct rq *rq_of_wrr_se(struct sched_wrr_entity *wrr_se) {
	struct task_struct *p = wrr_task_of(wrr_se);
	return task_rq(p);
}

/// @brief Get the WRR runqueue of a WRR scheduler entity.
static inline struct wrr_rq *wrr_rq_of_se(struct sched_wrr_entity *wrr_se) {
	struct rq *rq = rq_of_wrr_se(wrr_se);
	return &rq->wrr;
}

static inline int on_wrr_rq(struct sched_wrr_entity *wrr_se) {
	return wrr_se->on_rq;
}

/// @brief Increment runqueue variables after the enqueue.
static inline void inc_wrr_tasks(struct sched_wrr_entity *wrr_se, struct wrr_rq *wrr_rq) {
	wrr_rq->nr_running += 1;
	// TODO: SMP
}

/// @brief Decrement runqueue variables after the dequeue.
static inline void dec_wrr_tasks(struct sched_wrr_entity *wrr_se, struct wrr_rq *wrr_rq) {
	WARN_ON(!wrr_rq->nr_running);
	wrr_rq->nr_running -= 1;
	// TODO: SMP
}

/// @brief Enqueue a task to WRR runqueue.
/// @param rq a runqueue.
/// @param p a task to be enqueued to WRR runqueue of `rq`.
/// @param flags optional flags.
static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags) {
	struct sched_wrr_entity *wrr_se = &p->wrr;
	struct wrr_rq *wrr_rq = wrr_rq_of_se(wrr_se);

	// enqueue `wrr_se` to WRR runqueue
	if (move_entity(flags)) {
		// assume `wrr_se` is not in the queue
		WARN_ON_ONCE(wrr_se->on_rq);

		// if ENQUEUE_HEAD, insert `wrr_se` at the head
        // if !ENQUEUE_HEAD, insert `wrr_se` at the tail
		if (flags & ENQUEUE_HEAD)
            list_add(&wrr_se->run_list, &wrr_rq->queue);
        else
            list_add_tail(&wrr_se->run_list, &wrr_rq->queue);

		// set flag
		wrr_rq->bit = 1;
		wrr_se->on_rq = 1;

		// increment runqueue variables
		inc_wrr_tasks(wrr_se, wrr_rq);
		add_nr_running(rq, 1);
	}
}

/// @brief Dequeue a task from WRR runqueue.
/// @param rq a runqueue.
/// @param p a task to be dequeued from WRR runqueue of `rq`.
/// @param flags optional flags.
static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags) {
	struct sched_wrr_entity *wrr_se = &p->wrr;
	struct wrr_rq *wrr_rq = wrr_rq_of_se(wrr_se);

	// dequeue `wrr_se` from WRR runqueue
	if (on_wrr_rq(wrr_se) && move_entity(flags)) {
		// assume `wrr_se` is in the queue
		WARN_ON_ONCE(!wrr_se->on_rq);

		// dequeue `wrr_se`
		list_del_init(&wrr_se->run_list);

		// set flag
		if (list_empty(&wrr_rq->queue))
			wrr_rq->bit = 0;
		wrr_se->on_rq = 0;

		// decrement runqueue variables
		dec_wrr_tasks(wrr_se, wrr_rq);
		sub_nr_running(rq, 1);
	}
}

static void requeue_task_wrr(struct rq *rq, struct task_struct *p) {
	struct sched_wrr_entity *wrr_se = &p->wrr;
	struct wrr_rq *wrr_rq = &rq->wrr;

	if (on_wrr_rq(wrr_se)) {
		list_move(&wrr_se->run_list, &wrr_rq->queue);
	}
}

static void yield_task_wrr(struct rq *rq) {
	requeue_task_wrr(rq, rq->curr);
}

static void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags) {}

/// @brief Pick next task from WRR runqueue.
/// @param rq a runqueue.
/// @param prev previously executed task.
/// @param rf runqueue flags.
static struct task_struct *pick_next_task_wrr(struct rq *rq, struct task_struct *prev, struct rq_flags *rf) {
	struct wrr_rq *wrr_rq = &rq->wrr;
	struct task_struct *p;
	struct sched_wrr_entity *wrr_se;

	put_prev_task(rq, prev);

	wrr_se = list_entry(wrr_rq->queue.next, struct sched_wrr_entity, run_list);
	BUG_ON(!wrr_se);

	p = wrr_task_of(wrr_se);

	return p;
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *p) {}

#ifdef CONFIG_SMP
static int select_task_rq_wrr(struct task_struct *p, int task_cpu, int sd_flag, int flags) {
	return 0;
}

static void migrate_task_rq_wrr(struct task_struct *p, int new_cpu) {
	// WRR_TODO
}

static void task_woken_wrr(struct rq *this_rq, struct task_struct *task) {
	// WRR_TODO
}

static void rq_online_wrr(struct rq *rq) {
	// WRR_TODO
}

static void rq_offline_wrr(struct rq *rq) {
	// WRR_TODO
}
#endif

static void set_curr_task_wrr(struct rq *rq) {}

static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued) {
	struct sched_wrr_entity *wrr_se = &p->wrr;

	if (--wrr_se->time_slice)
		return;

	wrr_se->time_slice = wrr_se->weight * 10;

	if (wrr_se->run_list.prev != wrr_se->run_list.next) {
		requeue_task_wrr(rq, p);
		resched_curr(rq);
	}
}

/// @brief Return WRR timeslice based on task's weight.
static unsigned int get_rr_interval_wrr(struct rq *rq, struct task_struct *task) {
	return task->wrr.weight * 10;
}

static void prio_changed_wrr(struct rq *rq, struct task_struct *p, int oldprio) {}

static void switched_from_wrr(struct rq *rq, struct task_struct *p) {}

static void switched_to_wrr(struct rq *rq, struct task_struct *p) {}

static void update_curr_wrr(struct rq *rq) {}

const struct sched_class wrr_sched_class = {
	.next = &fair_sched_class,
	.enqueue_task = enqueue_task_wrr,
	.dequeue_task = dequeue_task_wrr,
	.yield_task = yield_task_wrr,
	.check_preempt_curr = check_preempt_curr_wrr,
	.pick_next_task = pick_next_task_wrr,
	.put_prev_task = put_prev_task_wrr,

#ifdef CONFIG_SMP
	.select_task_rq = select_task_rq_wrr,
	.migrate_task_rq = migrate_task_rq_wrr,
	.task_woken = task_woken_wrr,
	.set_cpus_allowed = set_cpus_allowed_common,
	.rq_online = rq_online_wrr,
	.rq_offline = rq_offline_wrr,
#endif

	.set_curr_task = set_curr_task_wrr,
	.task_tick = task_tick_wrr,
	.get_rr_interval = get_rr_interval_wrr,
	.prio_changed = prio_changed_wrr,
	.switched_from = switched_from_wrr,
	.switched_to = switched_to_wrr,
	.update_curr = update_curr_wrr,
};

static __latent_entropy void run_load_balance_wrr(struct softirq_action *h)
{
	struct rq *this_rq = this_rq();
	load_balance_wrr(this_rq);
}

/// find_busiest_queue 참고
static int load_balance_wrr(struct rq *rq)
{
	int cpu, max_cpu = -1, min_cpu = -1;
	struct rq *rq;
	int weight_sum, max_weight_sum = -1, min_weight_sum = -1;
	unsigned long next_balance = jiffies + msecs_to_jiffies(2000);

	// LB_TODO : Add lock for all CPUs. RCU?

	// Iterate over all online cpus
	for_each_online_cpu(cpu)
	{
		weight_sum = 0;
		rq = cpu_rq(cpu);

		rq_lock(rq);
		// Iterate over all tasks in the runqueue
		for_each_sched_wrr_entity(wrr_se, &rq->wrr) // LB_TODO : implement for_each_sched_wrr_entity
		{
			weight_sum += wrr_se->weight;
		}
		rq_unlock(rq);

		if (max_cpu == -1) // First online CPU
		{
			max_cpu = cpu;
			min_cpu = cpu;
			max_weight_sum = weight_sum;
			min_weight_sum = weight_sum;
		}
		else if (weight_sum > max_weight_sum)
		{
			max_cpu = cpu;
			max_weight_sum = weight_sum;
		}
		else if (weight_sum < min_weight_sum)
		{
			min_cpu = cpu;
			min_weight_sum = weight_sum;
		}
	}

	// LB_TODO: Continue implementing load balancing...
}

/*
 * Trigger the SCHED_SOFTIRQ_WRR if it is time to do periodic load balancing.
 */
void trigger_load_balance_wrr(struct rq *rq)
{
	/* Don't need to rebalance while attached to NULL domain */
	if (unlikely(on_null_domain(rq)))
		return;

	/* trigger_load_balance_wrr() checks a timer and if balancing is due, it fires the soft irq with the corresponding flag SCHED_SOFTIRQ_WRR. */
	if (time_after_eq(jiffies, rq->next_balance_wrr))
		raise_softirq(
			SCHED_SOFTIRQ_WRR);
}

__init void init_sched_wrr_class(void)
{
#ifdef CONFIG_SMP
	open_softirq(SCHED_SOFTIRQ_WRR, run_load_balance_wrr);
#endif /* SMP */
}
