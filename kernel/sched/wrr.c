/*
 * Weighted Round-Robin (WRR) Scheduling Class (mapped to the SCHED_WRR policy)
 */
#include "sched.h"

#define WRR_TIMESLICE 10

/// @brief Initialize a WRR runqueue.
/// @param wrr_rq a WRR runqueue to initiate.
void init_wrr_rq(struct wrr_rq *wrr_rq)
{
	INIT_LIST_HEAD(&wrr_rq->queue);
	wrr_rq->nr_running = 0;
	wrr_rq->total_weight = 0;
}

/// @brief Get the task_struct of a WRR scheduler entity.
/// @param wrr_se a WRR entity.
/// @return a task containing `wrr_se`.
static inline struct task_struct *wrr_task_of(struct sched_wrr_entity *wrr_se)
{
	return container_of(wrr_se, struct task_struct, wrr);
}

/// @brief Get the runqueue of a WRR runqueue.
/// @param wrr_rq a WRR runqueue.
/// @return a runqueue containing `wrr_rq`.
static inline struct rq *rq_of_wrr_rq(struct wrr_rq *wrr_rq)
{
	return container_of(wrr_rq, struct rq, wrr);
}

/// @brief Get the runqueue of a WRR scheduler entity.
/// @param wrr_se a WRR entity.
/// @return a runqueue on which `wrr_se` is enqueued.
static inline struct rq *rq_of_wrr_se(struct sched_wrr_entity *wrr_se)
{
	struct task_struct *p = wrr_task_of(wrr_se);
	return task_rq(p);
}

/// @brief Get the WRR runqueue of a WRR scheduler entity.
/// @param wrr_se a WRR entity.
/// @return a WRR runqueue on which `wrr_se` is enqueued.
static inline struct wrr_rq *wrr_rq_of_se(struct sched_wrr_entity *wrr_se)
{
	struct rq *rq = rq_of_wrr_se(wrr_se);
	return &rq->wrr;
}

/// @brief Check if a WRR scheduler entity is on a runqueue.
/// @param wrr_se a WRR entity.
/// @return 1 if `wrr_se` is on a WRR runqueue, else 0.
static inline int on_wrr_rq(struct sched_wrr_entity *wrr_se)
{
	return wrr_se->on_rq;
}

/// @brief Increment runqueue variables after the enqueue.
/// @param wrr_se a WRR entity.
/// @param wrr_rq a WRR runqueue.
static inline void inc_wrr_tasks(struct sched_wrr_entity *wrr_se, struct wrr_rq *wrr_rq)
{
	wrr_se->on_rq = 1;
	wrr_rq->nr_running += 1;
	wrr_rq->total_weight += wrr_se->weight;
}

/// @brief Decrement runqueue variables after the dequeue.
/// @param wrr_se a WRR entity.
/// @param wrr_rq a WRR runqueue.
static inline void dec_wrr_tasks(struct sched_wrr_entity *wrr_se, struct wrr_rq *wrr_rq)
{
	WARN_ON(!wrr_rq->nr_running);
	WARN_ON(!wrr_rq->total_weight);
	wrr_se->on_rq = 0;
	wrr_rq->nr_running -= 1;
	wrr_rq->total_weight -= wrr_se->weight;
}

/// @brief Enqueue a task to WRR runqueue.
/// @param rq a runqueue.
/// @param p a task to be enqueued to WRR runqueue of `rq`.
/// @param flags optional flags (not used).
static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;
	struct wrr_rq *wrr_rq = wrr_rq_of_se(wrr_se);

	if (on_wrr_rq(wrr_se))
		return;

	list_add_tail(&wrr_se->run_list, &wrr_rq->queue);

	inc_wrr_tasks(wrr_se, wrr_rq);
	add_nr_running(rq, 1);
}

/// @brief Dequeue a task from WRR runqueue.
/// @param rq a runqueue.
/// @param p a task to be dequeued from WRR runqueue of `rq`.
/// @param flags optional flags (not used).
static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;
	struct wrr_rq *wrr_rq = wrr_rq_of_se(wrr_se);

	if (!on_wrr_rq(wrr_se))
		return;

	list_del_init(&wrr_se->run_list);

	dec_wrr_tasks(wrr_se, wrr_rq);
	sub_nr_running(rq, 1);
}

/// @brief Dequeue a task from WRR runqueue, and enqueue it again.
/// @param rq a runqueue.
/// @param p a task to be requeued from WRR runqueue of `rq`.
static void requeue_task_wrr(struct rq *rq, struct task_struct *p)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;
	struct wrr_rq *wrr_rq = &rq->wrr;

	if (!on_wrr_rq(wrr_se))
		return;

	list_move(&wrr_se->run_list, &wrr_rq->queue);
}

/// @brief Requeue the current WRR task when yielding.
/// @param rq a runqueue.
static void yield_task_wrr(struct rq *rq) {
	requeue_task_wrr(rq, rq->curr);
}

/// @brief Pick the next task to execute from WRR runqueue.
/// @param rq a runqueue.
/// @param prev previously executed task.
/// @param rf runqueue flags (not used).
static struct task_struct *pick_next_task_wrr(struct rq *rq, struct task_struct *prev, struct rq_flags *rf)
{
	struct wrr_rq *wrr_rq = &rq->wrr;
	struct sched_wrr_entity *wrr_se;

	put_prev_task(rq, prev);

	wrr_se = list_first_entry_or_null(&wrr_rq->queue, struct sched_wrr_entity, run_list);
	if (!wrr_se)
		return NULL;

	return wrr_task_of(wrr_se);
}

/// @brief Requeue the previous WRR task on the runqueue.
/// @param rq a runqueue.
/// @param prev previously executed task.
static void put_prev_task_wrr(struct rq *rq, struct task_struct *prev)
{
	requeue_task_wrr(rq, prev);
}

#ifdef CONFIG_SMP
/// @brief Select a CPU to execute a task (with minimum total weight).
/// @param p a task to be enqueued in a runqueue.
/// @param cpu previously executed CPU index.
/// @param sd_flag sched-domain flag (not used).
/// @param wake_flags wake flags (not used).
static int select_task_rq_wrr(struct task_struct *p, int cpu, int sd_flag, int wake_flags)
{
	unsigned int min_cpu_index = -1;
	unsigned int min_total_weight = UINT_MAX;

	rcu_read_lock();
	for_each_online_cpu(cpu) {
		struct wrr_rq *wrr_rq = &cpu_rq(cpu)->wrr;
		if (cpumask_test_cpu(cpu, &p->cpus_allowed) && (wrr_rq->total_weight < min_total_weight)) {
			min_cpu_index = cpu;
			min_total_weight = wrr_rq->total_weight;
		}
	}
	rcu_read_unlock();

	return min_cpu_index;
}

static void migrate_task_rq_wrr(struct task_struct *p, int new_cpu)
{
	// WRR_TODO
}

static void task_woken_wrr(struct rq *this_rq, struct task_struct *task)
{
	// WRR_TODO
}

static void rq_online_wrr(struct rq *rq)
{
	// WRR_TODO
}

static void rq_offline_wrr(struct rq *rq)
{
	// WRR_TODO
}
#endif

/// @brief Update statistics of the current WRR task.
/// @param rq a runqueue.
static void update_curr_wrr(struct rq *rq)
{
	struct task_struct *curr = rq->curr;
	u64 delta_exec;
	u64 now;

	if (curr->sched_class != &wrr_sched_class)
		return;

	now = rq_clock_task(rq);
	delta_exec = now - curr->se.exec_start;
	if (unlikely((s64)delta_exec <= 0))
		return;

	schedstat_set(curr->se.statistics.exec_max,
		      max(curr->se.statistics.exec_max, delta_exec));

	curr->se.sum_exec_runtime += delta_exec;
	account_group_exec_runtime(curr, delta_exec);

	curr->se.exec_start = now;
	cgroup_account_cputime(curr, delta_exec);
}

/// @brief Update timeslice of the current WRR task at every tick.
/// @param rq a runqueue.
/// @param p the current task.
/// @param queued queued tick flag (not used).
static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;

	if (--wrr_se->time_slice)

	update_curr_wrr(rq);

	if (p->policy != SCHED_WRR)
		return;


	if (--wrr_se->time_slice) {
		return;
	}

	wrr_se->time_slice = wrr_se->weight * WRR_TIMESLICE;

	if (wrr_se->run_list.prev != wrr_se->run_list.next) {
		requeue_task_wrr(rq, p);
		resched_curr(rq);
	}
}

/// @brief Return the WRR timeslice based on task's weight.
/// @param rq a runqueue (not used).
/// @param task a task.
/// @return the WRR timeslice of `task`.
static unsigned int get_rr_interval_wrr(struct rq *rq, struct task_struct *task)
{
	return task->wrr.weight * WRR_TIMESLICE;
}

static void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags) {}

static void set_curr_task_wrr(struct rq *rq) {}

static void prio_changed_wrr(struct rq *rq, struct task_struct *p, int oldprio) {}

static void switched_from_wrr(struct rq *rq, struct task_struct *p) {}

static void switched_to_wrr(struct rq *rq, struct task_struct *p) {}

#ifdef CONFIG_SCHED_DEBUG
extern void print_wrr_rq(struct seq_file *m, int cpu, struct wrr_rq *wrr_rq);

/// @brief Print statistics of WRR scheduler.
/// @param m a sequence file to print the result.
/// @param cpu a CPU index.
void print_wrr_stats(struct seq_file *m, int cpu)
{
	rcu_read_lock();
	print_wrr_rq(m, cpu, &cpu_rq(cpu)->wrr);
	rcu_read_unlock();
}
#endif

const struct sched_class wrr_sched_class = {
	.next = &fair_sched_class,
	.enqueue_task = enqueue_task_wrr,
	.dequeue_task = dequeue_task_wrr,
	.yield_task = yield_task_wrr,
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

	.update_curr = update_curr_wrr,
	.task_tick = task_tick_wrr,
	.get_rr_interval = get_rr_interval_wrr,

	.check_preempt_curr = check_preempt_curr_wrr,
	.set_curr_task = set_curr_task_wrr,
	.prio_changed = prio_changed_wrr,
	.switched_from = switched_from_wrr,
	.switched_to = switched_to_wrr,
};

static __latent_entropy void run_load_balance_wrr(struct softirq_action *h)
{
	load_balance_wrr();
}

/// find_busiest_queue 참고
static void load_balance_wrr()
{
	int cpu, max_cpu = -1, min_cpu = -1;
	struct rq *rq;
	int weight_sum, max_weight_sum = -1, min_weight_sum = -1;
	unsigned long next_balance = jiffies + msecs_to_jiffies(2000);

	rcu_read_lock(); // LB_TODO: synchronization에 대해서는 좀 더 생각해 봐야 할듯... (일단은 rcu를 걸어두면 모든 CPU의 런큐 상태가 변하지 않는다는 가정 하에 짰음).. 근데 rq_lock도 추가해야할듯 왜냐면 fair에서 task migration시 rq_lock 하네

	// Iterate over all online cpus
	for_each_online_cpu(cpu)
	{
		weight_sum = 0;
		rq = cpu_rq(cpu);

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

	if (max_cpu == min_cpu) {
		rcu_read_unlock();
		return;
	}

	double_rq_lock(cpu_rq(max_cpu), cpu_rq(min_cpu));

	int max_weight = -1;
	struct sched_wrr_entity *wrr_se_max = NULL;
	// Choose the task with the highest weight on max_cpu
	for_each_sched_wrr_entity(wrr_se, &cpu_rq(max_cpu)->wrr)
	{
		if (wrr_se->weight > max_weight)
		{
			struct *task_struct p = wrr_task_of(wrr_se);

			/* If the task is currently running, continue */
			if (p == cpu_rq(max_cpu)->curr)
				continue;

			/* Migration should not make the total weight of min_cpu equal to or greater than that of max_cpu */
			if (min_weight_sum + wrr_se->weight >=
			    max_weight_sum - wrr_se->weight)
				continue;

			/* The task’s CPU affinity should allow migrating the task to min_cpu */
			if (!cpumask_test_cpu(min_cpu, &p->cpus_allowed))
				continue;

			/* All tests passed */
			max_weight = wrr_se->weight;
			wrr_se_max = wrr_se;
		}
	}

	/* No transferable task exists, return */
	if (wrr_se_max == NULL) {
		double_rq_unlock(cpu_rq(max_cpu), cpu_rq(min_cpu));
		rcu_read_unlock();
		return;
	}

	// LB_TODO: migrate



	double_rq_unlock(cpu_rq(max_cpu), cpu_rq(min_cpu));

	rcu_read_unlock();
}

/*
 * Trigger the SCHED_SOFTIRQ_WRR if it is time to do periodic load balancing.
 */
void trigger_load_balance_wrr()
{
	/* trigger_load_balance_wrr() checks a timer and if balancing is due, it fires the soft irq with the corresponding flag SCHED_SOFTIRQ_WRR. */
	if (time_after_eq(jiffies, rq->next_balance_wrr)) // LB_TODO: change next_balance tracker to global variable
		raise_softirq(SCHED_SOFTIRQ_WRR);
}

__init void init_sched_wrr_class(void)
{
#ifdef CONFIG_SMP
	open_softirq(SCHED_SOFTIRQ_WRR, run_load_balance_wrr);
#endif /* SMP */
}
