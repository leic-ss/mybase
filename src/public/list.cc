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

#include "list.h"

namespace mybase {

void list_push_front(struct clist* list, struct clist_elem* e)
{
    if (list->head == nullptr) {
        list->head = e;
        list->tail = e;
        e->next = e->prev = nullptr;
    } else {
        list->head->prev = e;
        e->prev = nullptr;
        e->next = list->head;
        list->head = e;
    }
    list->num_nodes++;
}

void list_push_back(struct clist* list, struct clist_elem* e)
{
    if (list->tail == nullptr) {
        list->head = e;
        list->tail = e;
        e->next = e->prev = nullptr;
    } else {
        list->tail->next = e;
        e->prev = list->tail;
        e->next = nullptr;
        list->tail = e;
    }
    list->num_nodes++;
}

struct clist_elem* list_remove(struct clist* list, struct clist_elem* e)
{
    if (!e) return nullptr;

    if (e->next) e->next->prev = e->prev;
    if (e->prev) e->prev->next = e->next;

    if (list->head == e) list->head = e->next;
    if (list->tail == e) list->tail = e->prev;

    list->num_nodes--;
    return e->next;
}

struct clist_elem* list_pop_front(struct clist* list)
{
    struct clist_elem *e = list->head;
    if (!e) return nullptr;

    if (e->next) e->next->prev = e->prev;
    if (e->prev) e->prev->next = e->next;

    if (list->head == e) list->head = e->next;
    if (list->tail == e) list->tail = e->prev;

    list->num_nodes--;
    return e;
}

struct clist_elem* list_pop_back(struct clist* list)
{
    struct clist_elem* e = list->tail;
    if (!e) return nullptr;

    if (e->next) e->next->prev = e->prev;
    if (e->prev) e->prev->next = e->next;

    if (list->head == e) list->head = e->next;
    if (list->tail == e) list->tail = e->prev;

    list->num_nodes--;
    return e;
}

}
