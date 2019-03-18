/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define IN
#define STATUS_FAILURE -1
#define STATUS_SUCCESS 0

typedef struct _item_t Item_t;
typedef struct _list_t List_t;
struct _item_t {
    Item_t *Previous;
    Item_t *Next;
    void *data;
};

struct _list_t {
    Item_t *First;
    Item_t *Last;
};

List_t *
InitializeList();
Item_t *
CreateItem();
int
InsertItemAtFront( IN List_t * TList, IN Item_t * kItem);
int
InsertItemBetweenItems( IN List_t * TList, IN Item_t * Previous, IN Item_t * Next, IN Item_t * Insert);
int
RemoveItem( IN List_t * TList, IN Item_t * kItem);
int
DestroyItemsInList(IN List_t * ItemList);
int
DestroyItemList(IN List_t * ItemList);
Item_t *
GetFirstItem(IN List_t * kItem);

Item_t *
GetLastItem(IN List_t * kItem);
List_t *
RemoveItemListBetweenItems( IN Item_t * Previous, IN Item_t * Next);

void
InsertItemListBetweenItems( IN Item_t * Previous, IN Item_t * Next, IN List_t * Insert);
