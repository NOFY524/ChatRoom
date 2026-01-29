#ifndef _LIST_H
#define _LIST_H

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - offsetof(type, member)))

struct list_head {
	struct list_head *prev, *next;
};

#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_safe(pos, n, head)                       \
	for (pos = (head)->next, n = pos->next; pos != (head); \
	     pos = n, n = n->next)

extern void INIT_LIST_HEAD(struct list_head *list);

extern void list_add_head(struct list_head *new, struct list_head *head);
extern void list_add_tail(struct list_head *new, struct list_head *head);

extern void list_del(struct list_head *entry);

extern int list_empty(struct list_head *head);

#endif