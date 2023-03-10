# This file is a part of Julia. License is MIT: https://julialang.org/license

# Date/DateTime Ranges

StepRange{<:Dates.DatePeriod,<:Real}(start, step, stop) =
    throw(ArgumentError("must specify step as a Period when constructing Dates ranges"))
Base.:(:)(a::T, b::T) where {T<:Date} = (:)(a, Day(1), b)

# Given a start and end date, how many steps/periods are in between
guess(a::DateTime, b::DateTime, c) = floor(Int64, (Int128(value(b)) - Int128(value(a))) / toms(c))
guess(a::Date, b::Date, c) = Int64(div(value(b - a), days(c)))
len(a::Time, b::Time, c) = Int64(div(value(b - a), tons(c)))
function len(a, b, c)
    lo, hi, st = min(a, b), max(a, b), abs(c)
    i = guess(a, b, c)
    v = lo + st * i
    prev = v  # Ensure `v` does not overflow
    while v <= hi && prev <= v
        prev = v
        v += st
        i += 1
    end
    return i - 1
end
Base.length(r::StepRange{<:TimeType}) = isempty(r) ? Int64(0) : len(r.start, r.stop, r.step) + 1
# Period ranges hook into Int64 overflow detection
Base.length(r::StepRange{<:Period}) = length(StepRange(value(r.start), value(r.step), value(r.stop)))
Base.checked_length(r::StepRange{<:Period}) = Base.checked_length(StepRange(value(r.start), value(r.step), value(r.stop)))

# Overload Base.steprange_last because `step::Period` may be a variable amount of time (e.g. for Month and Year)
function Base.steprange_last(start::T, step, stop) where T<:TimeType
    if isa(step, AbstractFloat)
        throw(ArgumentError("StepRange should not be used with floating point"))
    end
    z = zero(step)
    step == z && throw(ArgumentError("step cannot be zero"))

    if stop == start
        last = stop
    else
        if (step > z) != (stop > start)
            last = Base.steprange_last_empty(start, step, stop)
        else
            diff = stop - start
            if (diff > zero(diff)) != (stop > start)
                throw(OverflowError("Difference between stop and start overflowed"))
            end
            remain = stop - (start + step * len(start, stop, step))
            last = stop - remain
        end
    end
    return last
end

import Base.in
function in(x::T, r::StepRange{T}) where T<:TimeType
    n = len(first(r), x, step(r)) + 1
    n >= 1 && n <= length(r) && r[n] == x
end

Base.iterate(r::StepRange{<:TimeType}) = length(r) <= 0 ? nothing : (r.start, (length(r), 1))
Base.iterate(r::StepRange{<:TimeType}, (l, i)) = l <= i ? nothing : (r.start + r.step * i, (l, i + 1))

+(x::Period, r::AbstractRange{<:TimeType}) = (x + first(r)):step(r):(x + last(r))
+(r::AbstractRange{<:TimeType}, x::Period) = x + r
-(r::AbstractRange{<:TimeType}, x::Period) = (first(r)-x):step(r):(last(r)-x)
*(x::Period, r::AbstractRange{<:Real}) = (x * first(r)):(x * step(r)):(x * last(r))
*(r::AbstractRange{<:Real}, x::Period) = x * r
/(r::AbstractRange{<:P}, x::P) where {P<:Period} = (first(r)/x):(step(r)/x):(last(r)/x)

# Combinations of types and periods for which the range step is regular
Base.RangeStepStyle(::Type{<:OrdinalRange{<:TimeType, <:FixedPeriod}}) =
    Base.RangeStepRegular()
