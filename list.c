#include "list.h"

inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->prev = list->next = list;
}

static inline void __list_add(struct list_head *new, struct list_head *prev,
			      struct list_head *next)
{
	prev->next = new;
	next->prev = new;

	new->next = next;
	new->prev = prev;
}

inline void list_add_head(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

inline void list_del(struct list_head *entry)
{
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;
}

inline int list_empty(struct list_head *head)
{
	return head->next == head;
}