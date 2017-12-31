#define IN
#define STATUS_FAILURE -1
#define STATUS_SUCCESS 0

typedef struct _item_t Item_t;
typedef struct _list_t List_t;
struct _item_t
{
	Item_t		*Previous;
	Item_t		*Next;
	void		*data;
};

struct _list_t
{
	Item_t	*First;
	Item_t	*Last;
};

List_t * 
InitializeList();
Item_t *
CreateItem();
int
InsertItemAtFront(	IN List_t * TList,
			IN Item_t * kItem);
int
InsertItemBetweenItems(	IN List_t * TList,
				IN Item_t * Previous,
				IN Item_t * Next,
				IN Item_t * Insert);
int
RemoveItem(	IN List_t * 	TList,
		IN Item_t * 	kItem);
int
DestroyItemsInList(IN List_t * ItemList);
int
DestroyItemList(IN List_t * ItemList);
Item_t *
GetFirstItem(IN List_t * kItem);

Item_t *
GetLastItem(IN List_t * kItem);
List_t *
RemoveItemListBetweenItems(	IN Item_t *	Previous,
					IN Item_t *	Next);

void
InsertItemListBetweenItems(	IN Item_t *	Previous,
					IN Item_t *	Next,
					IN List_t *	Insert);
