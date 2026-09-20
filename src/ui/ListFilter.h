#pragma once
#include <QString>

#include "core/Item.h"

namespace mp {

struct ListFilter {
    enum Kind { All, Favorites, Trash, Type, Folder } kind = All;
    ItemType type = ItemType::Login;
    QString folderId;

    bool matches(const Item& item) const {
        switch (kind) {
            case All: return !item.trashed;
            case Favorites: return !item.trashed && item.favorite;
            case Trash: return item.trashed;
            case Type: return !item.trashed && item.type == type;
            case Folder: return !item.trashed && item.folderId == folderId;
        }
        return false;
    }
    bool operator==(const ListFilter& o) const {
        return kind == o.kind && type == o.type && folderId == o.folderId;
    }
};

}  // namespace mp
