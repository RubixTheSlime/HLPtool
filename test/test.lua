require 'coroutine'
require 'table'
require 'string'
require 'math'
require 'os'
local binaryheap = require( 'binaryheap')
--require('mobdebug').start()
math.randomseed(os.time())

initial_len = 15
max_pool_size = 300
initial_pool_size = 30
precount_timeout = 300
count_timeout = precount_timeout
verbose = true

function shuffle(tab)
   local keys = {}
   local values = {}
   local index = 1
   for k, v in pairs(tab) do
      keys[index] = k
      values[index] = v
      index = index + 1
   end
   for i=2,#values do
      index = math.random(0, i)
      values[index], values[1] = values[1], values[index]
   end
   for i=1,#values do
      tab[keys[i]] = values[i]
   end
end

function fn_to_str(fn)
   local res = ''
   for i=0,15 do
      res = res .. string.format('%x', fn[i])
   end
   return res
end

function get_init_pool()
   local function print_stat(count)
      if verbose then
         print(string.format('\x1b[1Acollect initial pool (length %d): %d / %d', initial_len, count, initial_pool_size))
      end
   end
   if verbose then print() end
   print_stat(0)
   local pool = {}
   while true do
      local fn = {}
      for i=0,15 do
         fn[i] = i
      end
      shuffle(fn)
      local len = #(hlp.solve({fn=fn}))
      if len == initial_len then
         local count = hlp.count_solve({fn=fn, depth=len, max_count=2})
         if count == 1 then
            pool[#pool + 1] = fn
            print_stat(#pool)
            if #pool == initial_pool_size then return pool end
         end
      end
   end
end

local function cmps(side)
   return coroutine.wrap(function()
      for barrel = 0, 15 do
         for _, sub in pairs({true, false}) do
            coroutine.yield({barrel = barrel, side = side, sub = sub})
         end
      end
   end)
end

function next_pool(pool)
   local set = {}
   for _, init_fn in ipairs(pool) do
      for cmp1 in cmps(true) do
         for cmp2 in cmps(false) do
            local new_fn = hlp.eval({{cmp1, cmp2}}, init_fn)
            set[fn_to_str(new_fn)] = new_fn
         end
      end
   end
   local new_pool = {}
   for _, fn in pairs(set) do
      new_pool[#new_pool + 1] = fn
   end
   shuffle(new_pool)
   return new_pool
end

function do_thing()
   local nexts = {}
   local heap = binaryheap.maxHeap(function(a, b)
      return a.count > b.count
   end)
   local min_count = math.huge
   local pool = get_init_pool()
   local len = initial_len
   local prefix_str = ''
   local function print_stat(progress)
      if verbose then
         local counts_str
         if heap:size() == 0 then
            counts_str = ''
         elseif min_count == heap:peek().count then
            counts_str = string.format(', badness %d', min_count-1)
         else
            counts_str = string.format(', badness %d - %d', min_count-1, heap:peek().count-1)
         end
         print(string.format('\x1b[1A%s%d / %d (%d pass%s)\x1b[K', prefix_str, progress, #nexts, heap:size(), counts_str))
      end
   end

   while true do
      len = len + 1
      prefix_str = string.format('extend to length %d: ', len)
      if verbose then print(string.format('%spreparing', prefix_str)) end
      nexts = next_pool(pool)
      min_count = math.huge
      for i, fn in pairs(nexts) do
         print_stat(i)
         local limit
         if heap:size() == max_pool_size then
            limit = heap:peek(1).count + 1
         end
         local function check(count)
            if count == nil then return false end
            if limit == nil then return true end
            return count < limit
         end
         local request = {fn=fn, depth=len, max_count=limit, accuracy='normal', timeout=precount_timeout}
         local precheck = true
         if limit == nil then
            precheck = hlp.solve({fn=fn, depth=len-1}) == nil
         else
            precheck = check(hlp.count_solve(request))
         end
         if precheck then
            request.accuracy='perfect'
            request.timeout = count_timeout
            local count = hlp.count_solve(request)
            if check(count) then
               if count < min_count then min_count = count end
               heap:insert({count=count, fn=fn})
               while heap:size() > max_pool_size and heap:peek() > min_count do
                  heap:remove(1)
               end
            end
         end
      end
      pool = {}
      while true do
         local item = heap:remove(1)
         if item == nil then break end
         pool[#pool + 1] = item.fn
      end

   end

   print('found the following of length ' .. (len - 1))
   local strings = {}
   for _, v in iparis(pool) do
      strings[#strings + 1] = fn_to_str(v)
   end
   print(table.unpack(strings))
   print('sanity check (exact solver):')
   for _, fn in ipairs(pool) do
      local chain = hlp.solve({fn=fn, accuracy='perfect'})
      print(fn_to_str(fn), #chain, hlp.chain_to_str(chain))
   end
end

function test_pi()
   local pi = {[0]=3,1,4,1,5,9,2,6,5,3,5,8,9,7,9,3}
   local result = hlp.count_solve({fn=pi, depth=13})
   print(result)
   --if (result ~= nil) then print(hlp.chain_to_str(result)) end
end

--test_pi()
do_thing()
