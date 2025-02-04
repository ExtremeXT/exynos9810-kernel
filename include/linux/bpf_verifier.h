/* Copyright (c) 2011-2014 PLUMgrid, http://plumgrid.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 */
#ifndef _LINUX_BPF_VERIFIER_H
#define _LINUX_BPF_VERIFIER_H 1

#include <linux/bpf.h> /* for enum bpf_reg_type */
#include <linux/filter.h> /* for MAX_BPF_STACK */

 /* Just some arbitrary values so we can safely do math without overflowing and
  * are obviously wrong for any sort of memory access.
  */
#define BPF_REGISTER_MAX_RANGE (1024 * 1024 * 1024)
#define BPF_REGISTER_MIN_RANGE -1

struct bpf_reg_state {
	enum bpf_reg_type type;
	union {
		/* valid when type == CONST_IMM | PTR_TO_STACK | UNKNOWN_VALUE */
		s64 imm;

		/* valid when type == PTR_TO_PACKET* */
		struct {
			u16 off;
			u16 range;
		};

		/* valid when type == CONST_PTR_TO_MAP | PTR_TO_MAP_VALUE |
		 *   PTR_TO_MAP_VALUE_OR_NULL
		 */
		struct bpf_map *map_ptr;
	};
	u32 id;
	/* Used to determine if any memory access using this register will
	 * result in a bad access. These two fields must be last.
	 * See states_equal()
	 */
	s64 min_value;
	u64 max_value;
	u32 min_align;
	u32 aux_off;
	u32 aux_off_align;
	bool value_from_signed;
};

enum bpf_stack_slot_type {
	STACK_INVALID,    /* nothing was stored in this stack slot */
	STACK_SPILL,      /* register spilled into stack */
	STACK_MISC	  /* BPF program wrote some data into this slot */
};

#define BPF_REG_SIZE 8	/* size of eBPF register in bytes */

/* state of the program:
 * type of all registers and stack info
 */
struct bpf_verifier_state {
	struct bpf_reg_state regs[MAX_BPF_REG];
	u8 stack_slot_type[MAX_BPF_STACK];
	struct bpf_reg_state spilled_regs[MAX_BPF_STACK / BPF_REG_SIZE];
};

/* linked list of verifier states used to prune search */
struct bpf_verifier_state_list {
	struct bpf_verifier_state state;
	struct bpf_verifier_state_list *next;
};

struct bpf_insn_aux_data {
	union {
		enum bpf_reg_type ptr_type;     /* pointer type for load/store insns */
		struct bpf_map *map_ptr;        /* pointer for call insn into lookup_elem */
	};
	int ctx_field_size; /* the ctx field size for load insn, maybe 0 */
	int converted_op_size; /* the valid value width after perceived conversion */
	int sanitize_stack_off; /* stack slot to be cleared */
	bool seen; /* this insn was processed by the verifier */
};

#define MAX_USED_MAPS 64 /* max number of maps accessed by one eBPF program */

#define BPF_VERIFIER_TMP_LOG_SIZE	1024

struct bpf_verifier_log {
	u32 level;
	char kbuf[BPF_VERIFIER_TMP_LOG_SIZE];
	char __user *ubuf;
	u32 len_used;
	u32 len_total;
};

static inline bool bpf_verifier_log_full(const struct bpf_verifier_log *log)
{
	return log->len_used >= log->len_total - 1;
}

static inline bool bpf_verifier_log_needed(const struct bpf_verifier_log *log)
{
	return log->level && log->ubuf && !bpf_verifier_log_full(log);
}

__printf(2, 3) void bpf_verifier_log_write(struct bpf_verifier_log *log,
					   const char *fmt, ...);

void bpf_verifier_vlog(struct bpf_verifier_log *log, const char *fmt,
		       va_list args);

struct bpf_verifier_env;
struct bpf_ext_analyzer_ops {
	int (*insn_hook)(struct bpf_verifier_env *env,
			 int insn_idx, int prev_insn_idx);
};

/* single container for all structs
 * one verifier_env per bpf_check() call
 */
struct bpf_verifier_env {
	struct bpf_prog *prog;		/* eBPF program being verified */
	const struct bpf_verifier_ops *ops;
	struct bpf_verifier_stack_elem *head; /* stack of verifier states to be processed */
	int stack_size;			/* number of states to be processed */
	bool strict_alignment;		/* perform strict pointer alignment checks */
	struct bpf_verifier_state cur_state; /* current verifier state */
	struct bpf_verifier_state_list **explored_states; /* search pruning optimization */
	const struct bpf_ext_analyzer_ops *analyzer_ops; /* external analyzer ops */
	const struct bpf_ext_analyzer_ops *dev_ops; /* device analyzer ops */
	void *analyzer_priv; /* pointer to external analyzer's private data */
	struct bpf_map *used_maps[MAX_USED_MAPS]; /* array of map's used by eBPF program */
	u32 used_map_cnt;		/* number of used maps */
	u32 id_gen;			/* used to generate unique reg IDs */
	bool allow_ptr_leaks;
	bool seen_direct_write;
	bool varlen_map_value_access;
	struct bpf_insn_aux_data *insn_aux_data; /* array of per-insn state */
};

#if defined(CONFIG_NET) && defined(CONFIG_BPF_SYSCALL)
int bpf_prog_offload_verifier_prep(struct bpf_verifier_env *env);
#else
int bpf_prog_offload_verifier_prep(struct bpf_verifier_env *env)
{
	return -EOPNOTSUPP;
}
#endif

int bpf_analyzer(struct bpf_prog *prog, const struct bpf_ext_analyzer_ops *ops,
		 void *priv);

#endif /* _LINUX_BPF_VERIFIER_H */
