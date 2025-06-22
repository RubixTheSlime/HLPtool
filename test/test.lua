require("table")
require("math")
fn_set = hlp.fn_set
hex_set = hlp.hex_set
hex_fn = hlp.hex_fn

to_test = {
    "Hex_fn",
    "Hex_set",
    "Fn_set",
    --"Initial_things"
}
local pi = hex_fn.new({ [0] = 3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9, 3 })

function Hex_fn()
    local pi2 = pi:clone()
    local identity = hex_fn.identity()
    local k5 = hex_fn.constant(5)
    local div2 = hex_fn.new(function(x) return math.floor(x / 2) end)

    print(pi, pi2, identity, k5, div2)
    -- 3141_5926_5358_9793    3141_5926_5358_9793    0123_4567_89AB_CDEF    5555_5555_5555_5555    0011_2233_4455_6677
    print(pi:compose(pi), pi:compose(k5), k5:compose(pi), k5:and_then(pi))
    -- 1151_9342_9195_3631    9999_9999_9999_9999    5555_5555_5555_5555    9999_9999_9999_9999
    print(pi:with(0, 7):with(4, 14))
    -- 7141_E926_5358_9793
    print(pi:is_constant(), identity:is_constant(), k5:is_constant(), k5:is_constant(3), k5:is_constant(5))
    -- false    false    true    false    true
    print(pi:is_identity(), identity:is_identity(), k5:is_identity())
    -- false    true    false
    print(pi == pi, pi == pi2, pi == identity)
    -- true    true    false
    print(pi:get(4), pi(4))
    -- 5    5
    pi:set(4, 1)
    print(pi(4), pi2(4))
    -- 1    5
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

function Hex_set()
    local primes = hex_set.from_values({ 2, 3, 5, 7, 11, 13 })
    local evens = hex_set.from_keys(function(x) return x % 2 == 0 end)
    local odds = evens:complement()
    local squares = hex_set.from_keys({[1] = true, [4] = true, [9] = true})
    local empty = hex_set.empty()
    local full = hex_set.full()
    local just7 = hex_set.single(7)
    print(primes, evens, odds, squares, empty, full, just7)
    print(#primes, #evens, #odds, #squares, #just7)
    print(primes:union(evens), primes:intersect(evens), primes:diff(evens), primes:sym_diff(evens))
    print(primes | evens, primes & evens, primes - evens, primes ~ evens)
    print(primes:is_empty(), primes:is_full(), empty:is_empty(), empty:is_full(), full:is_empty(), full:is_full())
    print("values")
    for _, x in ipairs(primes:to_values()) do
        print(x)
    end
    print("keys")
    for x in pairs(primes:to_keys()) do
        print(x)
    end

    print(primes:contains(3), primes:with(4):without(3):with(5, false):with(6, true))
    print(primes)
    local primes2 = primes:clone()
    print(primes2 == primes)
    primes:add(1)
    primes:remove(2)
    primes:add(3, false)
    primes:add(4, true)
    print(primes, primes2, primes == primes2)
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
    local near_proper_factors = fn_set.new(function(x, y)
        return (y ~= 0) and (x % y == 0) and ((x == 1) or (x ~= y))
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
    print("near_proper_factors", near_proper_factors)
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

    print(full:is_super(empty), factors:is_super(near_proper_factors), near_proper_factors:is_super(factors))
    print(factors == near_proper_factors, factors == multiples:inverse())
    local modpi = pi_set
        :with_io(1, hex_set.single(2))
        :with_oi(12, factors :get_io(12))
        :with_oi(1, hex_set.empty())
        :with(14, 13, true)
    print(modpi)
    print(pi_set:contains(pi), modpi:contains(pi), pi_set:contains_identity(), factors:contains_identity())

end

function Initial_things()
    local function bool_to_str(x)
        if (x) then return "true" else return "false" end
    end

    local print_chain = {}
    for i = 0, 15 do
        print_chain[i+1] = {{barrel=3, sub = i & 1 ~= 0, side = i & 4 ~= 0}, {barrel=5, sub = i & 2 ~= 0, side = i & 8 ~= 0}}
    end

    local function get_parts(half)
        return "{ barrel=" .. half.barrel .. "\tside=" .. bool_to_str(half.side) .. "\tsub=" .. bool_to_str(half.sub) .. " }"
    end

    local normalized = hlp.normalize_chain(print_chain)
    print("testing hlp.normalize_chain")
    for i, layer in ipairs(print_chain) do
        print(get_parts(layer[1]), get_parts(layer[2]), "->", get_parts(normalized[i][1]), get_parts(normalized[i][2]))
    end

    print("\ntesting hlp.chain_to_str")
    print('above in v notation:', hlp.chain_to_str(print_chain))
    print('above in star notation:', hlp.chain_to_str(print_chain, 'star'))

    local function print_table(tab)
        for i, v in pairs(tab) do
            print(i, v)
        end
    end

    local eval_chain = {
        { { barrel = 9, side = true, sub = false }, { barrel = 8, side = false, sub = true } },
        { { barrel = 4, side = true, sub = false }, { barrel = 4, side = false, sub = true } },
    }
    local eval_str = "(" .. hlp.chain_to_str(eval_chain) .. ")"

    print("\ntesting hlp.eval")
    print("output of " .. eval_str .. ":")
    print_table(hlp.eval(eval_chain))
    print("apply " .. eval_str .. " to pi:")
    print_table(hlp.eval(eval_chain, pi))

    print("\ntesting hlp.solve")
    local chain = hlp.solve(pi, false);
    print('solve pi:', hlp.chain_to_str(chain))
    local half_pi = {}
    for i = 0, 7 do
        half_pi[i] = pi[i]
    end

    local chain = hlp.solve(half_pi)

    local shifted = {}
    for i, v in pairs(hlp.eval(chain)) do
        shifted[i+1] = v
    end

    print('solve half pi (second half wildcards):', hlp.chain_to_str(chain), '( ' .. table.concat(shifted, "") .. ' )')
end



for i, s in ipairs(to_test) do
    if i ~= 1 then
        print()
        print("--------------------------------------------------------------------------------")
    end
    print("testing: " .. s)
    _G[s]()
end
