/*
 * Algorithms
 *
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
 *
 * This file is part of the NOVA microhypervisor.
 *
 * NOVA is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NOVA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 */

#pragma once

#include "util.hpp"

template <typename IT, typename IT_END, typename T>
T accumulate (IT begin, IT_END end, T init)
{
    for (; begin != end; ++begin) {
        init += *begin;
    }

    return init;
}

template <typename T, typename VAL>
VAL accumulate (T const &container, VAL &&init)
{
    return accumulate (container.begin(), container.end(), forward<VAL> (init));
}

template <typename IT, typename IT_END, typename PRED>
IT find_if (IT begin, IT_END end, PRED predicate)
{
    for (; begin != end and not predicate (*begin); ++begin) {
    }

    return begin;
}

template <typename T, typename PRED>
auto find_if (T const &container, PRED &&predicate)
{
    return find_if (container.begin(), container.end(), forward<PRED> (predicate));
}

template <typename IT, typename IT_END, typename FN>
void for_each (IT begin, IT_END end, FN &&fn)
{
    for (; begin != end; ++begin) {
        fn (*begin);
    }
}

template <typename T, typename FN>
void for_each (T const &container, FN &&fn)
{
    for_each (container.begin(), container.end(), forward<FN> (fn));
}
