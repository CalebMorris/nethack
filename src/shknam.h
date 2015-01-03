#ifndef SHKNAM_H
#define SHKNAM_H

#include "artifact_names.h"
#include "monst.h"
#include "onames.h"

void stock_room(int,struct mkroom *);
bool saleable(struct monst *,struct obj *);
int get_shop_item(int);

#endif // SHKNAM_H
