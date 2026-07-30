/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#include <sys/types.h>
#include <sys/list.h>
#include <sys/sysmacros.h>

struct common_test_node {
	int value;
	list_node_t link;
};

/* Verify a real genunix container object, not a bootstrap reimplementation. */
int
arm64_common_selftest(void)
{
	struct common_test_node first = { .value = 1 };
	struct common_test_node second = { .value = 2 };
	struct common_test_node third = { .value = 3 };
	list_t list;

	list_create(&list, sizeof (struct common_test_node),
	    offsetof(struct common_test_node, link));
	list_insert_head(&list, &second);
	list_insert_before(&list, &second, &first);
	list_insert_tail(&list, &third);
	if (list_head(&list) != &first || list_next(&list, &first) != &second ||
	    list_tail(&list) != &third)
		return (-1);
	if (list_remove_head(&list) != &first ||
	    list_remove_tail(&list) != &third ||
	    list_remove_head(&list) != &second || !list_is_empty(&list))
		return (-1);
	list_destroy(&list);
	return (0);
}
