/*
 * Copyright (C) 2022-2025 Colin Ian King.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "stress-ng.h"
#include "core-attribute.h"
#include "core-builtin.h"
#include "core-numa.h"

#if defined(HAVE_LINUX_MEMPOLICY_H)
#include <linux/mempolicy.h>
#endif

static const char option[] = "option --mbind";

/*
 *  stress_numa_count_mem_nodes()
 *	determine the number of NUMA memory nodes
 */
unsigned long int stress_numa_count_mem_nodes(unsigned long int *max_node)
{
	FILE *fp;
	unsigned long int node_id = 0;
	char buffer[8192], *ptr;
	const char *str = NULL;
	long int n = 0;

	*max_node = 0;

	fp = fopen("/proc/self/status", "r");
	if (!fp)
		return -1;

	while (fgets(buffer, sizeof(buffer), fp)) {
		if (!strncmp(buffer, "Mems_allowed:", 13)) {
			str = buffer + 13;
			break;
		}
	}
	(void)fclose(fp);

	if (!str)
		return -1;

	ptr = buffer + strlen(buffer) - 2;

	/*
	 *  Parse hex digits into NUMA node ids, these
	 *  are listed with least significant node last
	 *  so we need to scan backwards from the end of
	 *  the string back to the start.
	 */
	while ((*ptr != ' ') && (ptr > str)) {
		int i;
		unsigned int val;

		/* Skip commas */
		if (*ptr == ',') {
			ptr--;
			continue;
		}

		if (sscanf(ptr, "%1x", &val) != 1)
			return -1;

		/* Each hex digit represent 4 memory nodes */
		for (i = 0; i < 4; i++) {
			if (val & (1 << i))
				n++;
			node_id++;
			if (*max_node < node_id)
				*max_node = node_id;
		}
		ptr--;
	}

	return n;
}

/*
 *  stress_numa_mask_alloc()
 *	allocate numa mask
 */
stress_numa_mask_t *stress_numa_mask_alloc(void)
{
	stress_numa_mask_t *numa_mask;

	numa_mask = malloc(sizeof(*numa_mask));
	if (!numa_mask)
		return NULL;

	numa_mask->nodes = stress_numa_count_mem_nodes(&numa_mask->max_nodes);
	if ((numa_mask->nodes < 1) || (numa_mask->max_nodes < 1)) {
		free(numa_mask);
		return NULL;
	}

	/* number of longs based on maximum number of nodes */
	numa_mask->numa_elements = (numa_mask->max_nodes + NUMA_LONG_BITS - 1) / NUMA_LONG_BITS;
	numa_mask->numa_elements = numa_mask->numa_elements ? numa_mask->numa_elements : 1;
	/* size of mask in bytes */
	numa_mask->mask_size = (size_t)(NUMA_LONG_BITS * numa_mask->numa_elements) / BITS_PER_BYTE;
	/* allocated mask */
	numa_mask->mask = (unsigned long int *)calloc(numa_mask->mask_size, 1);

	if (!numa_mask->mask) {
		free(numa_mask);
		return NULL;
	}
	return numa_mask;
}

/*
 *  stress_numa_mask_free()
 *	free numa_mask and mask
 */
void stress_numa_mask_free(stress_numa_mask_t *numa_mask)
{
	if (!numa_mask)
		return;
	free(numa_mask->mask);
	free(numa_mask);
}

#if defined(__NR_get_mempolicy) &&      \
    defined(__NR_mbind) &&              \
    defined(__NR_migrate_pages) &&      \
    defined(__NR_move_pages) &&         \
    defined(__NR_set_mempolicy) &&	\
    defined(HAVE_LINUX_MEMPOLICY_H)

/*
 * stress_check_numa_range()
 * @max_node: maximum NUMA node allowed, 0..N
 * @node: node number to check
 */
static void stress_check_numa_range(
	const unsigned long int max_node,
	const unsigned long int node)
{
	if (node >= max_node) {
		if (max_node > 1) {
			(void)fprintf(stderr, "%s: invalid range, %lu is not allowed, "
				"allowed range: 0 to %lu\n", option,
				node, max_node - 1);
		} else {
			(void)fprintf(stderr, "%s: invalid range, %lu is not allowed, "
				"allowed range: 0\n", option, node);
		}
		_exit(EXIT_FAILURE);
	}
}

/*
 *  stress_numa_randomize_pages()
 *	randomize NUMA node for pages in buffer
 */
void stress_numa_randomize_pages(
	stress_numa_mask_t *numa_mask,
	void *buffer,
	const size_t page_size,
	const size_t buffer_size)
{
	uint8_t *ptr, *ptr_end = (uint8_t *)buffer + buffer_size;

	(void)shim_memset(numa_mask->mask, 0, numa_mask->mask_size);
	for (ptr = (uint8_t *)buffer; ptr < ptr_end; ptr += page_size) {
		const unsigned long int node = (unsigned long int)stress_mwc32modn((uint32_t)numa_mask->nodes);

		STRESS_SETBIT(numa_mask->mask, (unsigned long int)node);
		(void)shim_mbind((void *)ptr, page_size, MPOL_BIND, numa_mask->mask,
                        numa_mask->max_nodes, MPOL_MF_MOVE);
		STRESS_CLRBIT(numa_mask->mask, (unsigned long int)node);
	}
}

/*
 *  stress_numa_nodes()
 *	determine the number of NUMA memory nodes,
 *	always returns at least 1 if no nodes found,
 *	useful for cache size scaling by node count
 */
unsigned long int stress_numa_nodes(void)
{
	unsigned long int max_node = 0;
	static int nodes = -1;	/* used as a cached copy */

	if (nodes == -1) {
		nodes = stress_numa_count_mem_nodes(&max_node);
		if (nodes < 1)
			nodes = 1;
	}
	return nodes;
}

/*
 * stress_parse_node()
 * @str: parse string containing decimal NUMA node number
 *
 * Returns: NUMA node number, or exits the program on invalid number in str
 */
static unsigned long int stress_parse_node(const char *const str)
{
	unsigned long int val;

	if (sscanf(str, "%lu", &val) != 1) {
		(void)fprintf(stderr, "%s: invalid number '%s'\n", option, str);
		_exit(EXIT_FAILURE);
	}
	return val;
}

/*
 * stress_set_mbind()
 * @arg: list of NUMA nodes to bind to, comma separated
 *
 * Returns: 0 - OK
 */
int stress_set_mbind(const char *arg)
{
	char *str, *ptr, *token;
	unsigned long int max_node;
	unsigned long int *nodemask;
	const size_t nodemask_bits = sizeof(*nodemask) * 8;
	size_t nodemask_sz;

	if (stress_numa_count_mem_nodes(&max_node) < 1) {
		(void)fprintf(stderr, "no NUMA nodes found, ignoring --mbind setting '%s'\n", arg);
		return 0;
	}

	nodemask_sz = (max_node + (nodemask_bits - 1)) / nodemask_bits;
	nodemask = (unsigned long int *)calloc(nodemask_sz, sizeof(*nodemask));
	if (!nodemask) {
		(void)fprintf(stderr, "parsing --mbind: cannot allocate NUMA nodemask, out of memory\n");
		_exit(EXIT_FAILURE);
	}

	str = stress_const_optdup(arg);
	if (!str) {
		(void)fprintf(stderr, "out of memory duplicating argument '%s'\n", arg);
		_exit(EXIT_FAILURE);
	}

	for (ptr = str; (token = strtok(ptr, ",")) != NULL; ptr = NULL) {
		unsigned long int i, lo, hi;
		char *tmpptr = strstr(token, "-");

		hi = lo = stress_parse_node(token);
		if (tmpptr) {
			tmpptr++;
			if (*tmpptr)
				hi = stress_parse_node(tmpptr);
			else {
				(void)fprintf(stderr, "%s: expecting number following "
					"'-' in '%s'\n", option, token);
				free(str);
				_exit(EXIT_FAILURE);
			}
			if (hi <= lo) {
				(void)fprintf(stderr, "%s: invalid range in '%s' "
					"(end value must be larger than "
					"start value\n", option, token);
				free(str);
				_exit(EXIT_FAILURE);
			}
		}
		stress_check_numa_range(max_node, lo);
		stress_check_numa_range(max_node, hi);

		for (i = lo; i <= hi; i++) {
			STRESS_SETBIT(nodemask, i);
			if (shim_set_mempolicy(MPOL_BIND, nodemask, max_node) < 0) {
				(void)fprintf(stderr, "%s: could not set NUMA memory policy for node %lu, errno=%d (%s)\n",
					option, i, errno, strerror(errno));
				free(str);
				_exit(EXIT_FAILURE);
			}
		}
	}

	free(nodemask);
	free(str);
	return 0;
}

#else
void stress_numa_randomize_pages(
	stress_numa_mask_t *numa_mask,
	void *buffer,
	const size_t page_size,
	const size_t buffer_size)
{
	(void)numa_mask;
	(void)buffer;
	(void)page_size;
	(void)buffer_size;

	(void)shim_memset(numa_mask->mask, 0, numa_mask->mask_size);
}

unsigned long int PURE stress_numa_nodes(void)
{
	return 1;
}

int stress_set_mbind(const char *arg)
{
	(void)arg;

	(void)fprintf(stderr, "%s: setting NUMA memory policy binding not supported\n", option);
	_exit(EXIT_FAILURE);
}
#endif
