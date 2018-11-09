/*-
 * Copyright (c) 2018 The UbixOS Project.
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

#include <sys/types.h>
#include <lib/kmalloc.h>
#include <lib/kern_trie.h>

struct Trie *new_trieNode() {

  struct Trie *node = (struct Trie *) kmalloc(sizeof(struct Trie));

  node->isLeaf = 0;

  for (int i = 0; i < CHAR_SIZE; i++)
    node->character[i] = NULL;

  return (node);
}

// Insert Trie
void insert_trieNode(struct Trie **head, char* str, void *e) {

  // start from root node
  struct Trie* curr = *head;

  while (*str) {

    // create a new node if path doesn't exists
    if (curr->character[*str - 'a'] == NULL) {
      curr->character[*str - 'a'] = new_trieNode();
    }

    // go to next node
    curr = curr->character[*str - 'a'];
    // move to next character
    str++;
  }

  curr->e = e;
  // mark current node as leaf
  curr->isLeaf = 1;
}


struct Trie *search_trieNode(struct Trie *head, char *str) {

  // return 0 if Trie is empty
  if (head == NULL)
    return (0);

  struct Trie *curr = head;

  while (*str) {

    // go to next node
    curr = curr->character[*str - 'a'];

    // if string is invalid (reached end of path in Trie)
    if (curr == NULL)
      return (0);

    // move to next character
    str++;
  }

  // if current node is a leaf and we have reached the
  // end of the string, return 1
  return (curr);
}



// Recursive function to delete a string in Trie
int delete_trieNode(struct Trie **curr, char *str) {

  // return if Trie is empty
  if (*curr == NULL)
    return (0);

  // if we have not reached the end of the string
  if (*str) {
    // recurse for the node corresponding to next character in
    // the string and if it returns 1, delete current node
    // (if it is non-leaf)
    if (*curr != NULL && (*curr)->character[*str - 'a'] != NULL && deletion(&((*curr)->character[*str - 'a']), str + 1) && (*curr)->isLeaf == 0) {
      if (!haveChildren(*curr)) {
        free(*curr);
        (*curr) = NULL;
        return (1);
      }
      else {
        return (0);
      }
    }
  }

  // if we have reached the end of the string
  if (*str == '\0' && (*curr)->isLeaf) {
    // if current node is a leaf node and don't have any children
    if (!haveChildren(*curr)) {
      free(*curr); // delete current node
      (*curr) = NULL;
      return (1); // delete non-leaf parent nodes
    }

    // if current node is a leaf node and have children
    else {
      // mark current node as non-leaf node (DON'T DELETE IT)
      (*curr)->isLeaf = 0;
      return (0);    // don't delete its parent nodes
    }
  }

  return (0);
}

// returns 1 if given node has any children
int haveChildren(struct Trie* curr) {
  for (int i = 0; i < CHAR_SIZE; i++)
    if (curr->character[i])
      return (1); // child found

  return (0);
}
