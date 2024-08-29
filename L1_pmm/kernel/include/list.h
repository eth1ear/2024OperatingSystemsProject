#ifndef _LIST_H
#define _LIST_H

struct list_head {
    struct list_head *prev;
    struct list_head *next;
};

static inline void init_list_head(struct list_head *list) {
    list->next = list;
    list->prev = list;
}

static inline void list_add(struct list_head *node, struct list_head *head) {
    node->next = head->next;
    node->prev = head;
    head->next->prev = node;
    head->next = node;
}

static inline void list_append(struct list_head *node, struct list_head *head) {
    struct list_head *tail = head->prev;
    return list_add(node, tail);
}

static inline void list_del(struct list_head *node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

static inline int list_empty(struct list_head *head) { return (head->prev == head && head->next == head); }

#endif