/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/HashTable.h>
#include <LibJS/Runtime/GlobalObject.h>
#include <LibJS/Runtime/Object.h>

namespace JS {

class WeakSet : public Object {
    JS_OBJECT(WeakSet, Object);

public:
    static WeakSet* create(GlobalObject&);

    explicit WeakSet(Object& prototype);
    virtual ~WeakSet() override;

    HashTable<Cell*> const& values() const { return m_values; };
    HashTable<Cell*>& values() { return m_values; };

    void remove_sweeped_cells(Badge<Heap>, Vector<Cell*>&);

private:
    HashTable<Cell*> m_values; // This stores Cell pointers instead of Object pointers to aide with sweeping
};

}
