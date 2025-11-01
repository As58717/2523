#pragma once

#if __has_include("Templates/Optional.h")
#include "Templates/Optional.h"
#else
#include <optional>

/**
 * Minimal fallback implementation of Unreal's TOptional backed by std::optional.
 * This keeps the plugin compatible with engine versions where Templates/Optional.h
 * has been relocated or removed from the public headers.
 */
template <typename ValueType>
class TOptional : public std::optional<ValueType>
{
public:
    using std::optional<ValueType>::optional;

    bool IsSet() const
    {
        return this->has_value();
    }

    void Reset()
    {
        this->reset();
    }

    ValueType& GetValue()
    {
        return this->value();
    }

    const ValueType& GetValue() const
    {
        return this->value();
    }
};
#endif
