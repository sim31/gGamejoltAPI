#include "register_types.h"
#include "object_type_db.h"
#include "gamejoltAPI.h"

void register_gamejoltAPI_types() 
{
    ObjectTypeDB::register_type<GamejoltAPI>();
}

void unregister_gamejoltAPI_types() 
{
   //nothing to do here
}
