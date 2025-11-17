require("table")
require("math")
fn_set = hlp.fn_set
hex_set = hlp.hex_set
hex_fn = hlp.hex_fn
hex_layer = hlp.hex_layer

to_test = {
    "Hex_fn",
    "Hex_set",
    "Hex_layer",
    "Fn_set",
}
local pi = hex_fn.new({ [0] = 3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9, 3 })

function Hex_fn()
    local pi2 = pi:clone()
    local identity = hex_fn.identity()
    local k5 = hex_fn.constant(5)
    local div2 = hex_fn.new(function(x) return math.floor(x / 2) end)

    print(pi, pi2, identity, k5, div2)
    print(pi:compose(pi), pi:compose(k5), k5:compose(pi), k5:and_then(pi))
    print(pi:with(0, 7):with(4, 14))
    print(pi:is_constant(), identity:is_constant(), k5:is_constant(), k5:is_constant(3), k5:is_constant(5))
    print(pi:is_identity(), identity:is_identity(), k5:is_identity())
    print(pi == pi, pi == pi2, pi == identity)
    print(pi:get(4), pi(4))
    pi:set(4, 1)
    print(pi(4), pi2(4))
    print(pi:out_set(), identity:out_set(), k5:out_set(), div2:out_set())
end

function Gcd(x, y)
    if (x < y) then return Gcd(y, x) end
    if (y == 0) then return x end
    return Gcd(y, x % y)
end

function IsPrime(x)
    if x < 2 then
        return false
    end
    for i = 2, x - 1 do
        if x % i == 0 then
            return false
        end
    end
    return true
end

function IsComposite(x)
    return x ~= 1 and not IsPrime(x)
end

-- note: during the current release, this is effectively useless
function Hex_set()
    local primes = hex_set.from_values({ 2, 3, 5, 7, 11, 13 })
    local evens = hex_set.from_keys(function(x) return x % 2 == 0 end)
    local odds = evens:complement()
    local squares = hex_set.from_keys({[1] = true, [4] = true, [9] = true})
    local empty = hex_set.empty()
    local full = hex_set.full()
    local just7 = hex_set.single(7)
    print(primes, evens, odds, squares, empty, full, just7)
    print(#primes, #evens, #odds, #squares, #empty, #full, #just7)
    print(primes:union(evens), primes:intersect(evens), primes:diff(evens), primes:sym_diff(evens))
    print(primes | evens, primes & evens, primes - evens, primes ~ evens)
    print(primes:is_empty(), primes:is_full(), empty:is_empty(), empty:is_full(), full:is_empty(), full:is_full())
    print("values", table.concat(primes:to_values(), ", "))
    local tab = {}
    for x in pairs(primes:to_keys()) do tab[#tab+1] = x end
    print("keys", table.concat(tab, ", "))

    print(primes:contains(3), primes:with(4):without(3):with(5, false):with(6, true))
    local primes2 = primes:clone()
    print(primes, primes2, primes2 == primes)
    primes2:add(1)
    primes2:remove(2)
    primes2:add(3, false)
    primes2:add(4, true)
    print(primes, primes2, primes == primes2)
end

function Count(iter)
    local count = 0
    for _ in iter do count = count + 1 end
    return count
end

function Hex_layer()
    print('all', Count(hex_layer.iter()))
    print('unique', Count(hex_layer.iter(true)))
    for i = 4, 6 do
        local function filter(layer)
            return layer:is_classic(i)
        end
        print('all, classic ' .. i, Count(hex_layer.iter(false, filter)))
        print('unique, classic ' .. i, Count(hex_layer.iter(true, filter)))
    end
    for layer in hex_layer.iter(false, function(layer)
        local b1 = layer:get_barrel(1)
        local b2 = layer:get_barrel(2)
        return layer:is_classic(6) and (b1 == 3 and b2 == 9 or b1 == 9 and b2 == 3)
    end) do
        local alts = {}
        for alt in layer:alts(true) do
            alts[#alts+1] = tostring(alt)
        end
        print(tostring(layer) .. '  ', layer:is_classic(4), layer:is_classic(5), layer:get_fn(), #alts, table.concat(alts, ";  "))
    end
    print(hex_layer.of(3, true, false, 5, true, false))
    print(hex_layer.of_classic(3, true, 5, false))
    print(hex_layer.of_classic(7, true, 4, true))
    local layer = hex_layer.of_classic(8, true, 9, false)
    print(layer, layer:get_barrel(1), layer:get_sub(1), layer:get_neg(1), layer:get_barrel(2), layer:get_sub(2), layer:get_neg(2))
end


function Fn_set()
    local identity = fn_set.identity()
    local empty = fn_set.empty()
    local full = fn_set.full()
    local factors = fn_set.new(function(x, y)
        if y == 0 then return x == 0 end
        return x % y == 0
    end)
    local multiples = factors:inverse()
    local proper_factors = fn_set.new(function(x, y)
        return (y ~= 0) and (x % y == 0) and y < x
    end)
    local primality = fn_set.new(function(x, y)
        if IsPrime(x) and IsPrime(y) then return x ~= y end
        if IsComposite(x) and IsComposite(y) then return x ~= y end
        return x == 1 and y == 1
    end)
    local coprimes = fn_set.new(function(x, y)
        return Gcd(x, y) == 1
    end)
    local pi_set = fn_set.single(pi)
    print("identity", identity)
    print("empty", empty)
    print("full", full)
    print("factors 1", factors)
    print("factors 2", multiples:inverse())
    print("multiples", multiples)
    print("proper_factors", proper_factors)
    print("same_primality_inequal", primality)
    print("coprime", coprimes)

    print("mul/div factors, pi")
    print("pre_mul", factors:pre_mul(pi))
    print("post_mul", factors:post_mul(pi))
    print("pre_div", factors:pre_div(pi))
    print("post_div", factors:post_div(pi))
    print("pre_mul + pre_div", factors:pre_mul(pi):pre_div(pi))
    print("post_mul + post_div", factors:post_mul(pi):post_div(pi))
    print("pre_div + pre_mul", factors:pre_div(pi):pre_mul(pi))
    print("post_div + post_mul", factors:post_div(pi):post_mul(pi))
    print(full:is_super(empty), factors:is_super(proper_factors), proper_factors:is_super(factors))
    print(factors == proper_factors, factors == multiples:inverse())
    local modpi = pi_set
        :with_io(1, hex_set.single(2))
        :with_oi(12, factors:get_io(12))
        :with_oi(1, hex_set.empty())
        :with(14, 13, true)
    print(modpi)
    print(pi_set:contains(pi), modpi:contains(pi), pi_set:contains_identity(), factors:contains_identity())
    print(pi_set:out_set(), proper_factors:out_set())
    print(pi_set:sat(), empty:sat(), proper_factors:sat())
end

for i, s in ipairs(to_test) do
    if i ~= 1 then
        print()
        print("--------------------------------------------------------------------------------")
    end
    print("testing: " .. s)
    _G[s]()
end
