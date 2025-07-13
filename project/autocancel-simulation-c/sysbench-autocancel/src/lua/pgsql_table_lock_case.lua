require("oltp_common")

function prepare_statements()
   if not sysbench.opt.skip_trx then
      prepare_begin()
      prepare_commit()
   end

   prepare_point_selects()

   if sysbench.opt.range_selects then
      prepare_simple_ranges()
      prepare_sum_ranges()
      prepare_order_ranges()
      prepare_distinct_ranges()
   end

   prepare_index_updates()
   prepare_non_index_updates()
   prepare_delete_inserts()
end

start_time = nil

function event()

   if not start_time then
      start_time = os.time()
   end

   if not sysbench.opt.skip_trx then
      begin()
   end

   local current_time = os.time()
   local time_interval = current_time - start_time

   if time_interval < 65 then
      execute_point_selects()

      if sysbench.opt.range_selects then
         execute_simple_ranges()
         execute_sum_ranges()
         execute_order_ranges()
         execute_distinct_ranges()
      end

      execute_index_updates()
      execute_non_index_updates()
   elseif time_interval >= 65 and time_interval <= 95 then
      os.execute("sleep " .. tonumber(31))

      execute_point_selects_under_table_id(25)

      if sysbench.opt.range_selects then
         execute_simple_ranges_under_table_id(25)
         execute_sum_ranges_under_table_id(25)
         execute_order_ranges_under_table_id(25)
         execute_distinct_ranges_under_table_id(25)
      end

      execute_index_updates_under_table_id(25)
   else
      execute_point_selects_under_table_id(25)

      if sysbench.opt.range_selects then
         execute_simple_ranges_under_table_id(25)
         execute_sum_ranges_under_table_id(25)
         execute_order_ranges_under_table_id(25)
         execute_distinct_ranges_under_table_id(25)
      end

      execute_index_updates_under_table_id(25)
   end


   if not sysbench.opt.skip_trx then
      commit()
   end

   check_reconnect()
end