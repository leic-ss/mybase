/************************************************************************
Copyright 2020 ~ 2021
Author: zhanglei

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
 
    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
**************************************************************************/

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mybase {

struct clist_elem {
    struct clist_elem* prev{nullptr};
    struct clist_elem* next{nullptr};
};

struct clist {
    struct clist_elem* head{nullptr};
    struct clist_elem* tail{nullptr};
    uint32_t num_nodes{0};
};

#ifndef _get_entry
#define _get_entry(ELEM, STRUCT, MEMBER) \
    ((STRUCT *) ((uint8_t *) (ELEM) - offsetof (STRUCT, MEMBER)))
#endif

static inline size_t list_size(struct clist* list) {
    return list->num_nodes;
}
static inline struct clist_elem* list_begin(struct clist* list) {
    return list->head;
}
static inline struct clist_elem* list_end(struct clist* list) {
    return list->tail;
}
static inline struct clist_elem* list_next(struct clist_elem* e) {
    return e->next;
}
static inline struct clist_elem* list_prev(struct clist_elem* e) {
    return e->prev;
}
static inline int list_is_empty(struct clist* list) {
    return list->head == nullptr;
}

// Insert `e` at the head of `list`.
void list_push_front(struct clist* list, struct clist_elem* e);

// Insert `e` at the tail of `list`.
void list_push_back(struct clist* list, struct clist_elem* e);

// Remove `e`, and return its next.
struct clist_elem* list_remove(struct clist* list, struct clist_elem* e);

// Remove the head of `list`, and then return it.
struct clist_elem* list_pop_front(struct clist* list);

// Remove the tail of `list`, and then return it.
struct clist_elem* list_pop_back(struct clist* list);

}
