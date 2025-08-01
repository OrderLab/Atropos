require("oltp_common")

function prepare_statements()
   prepare_point_selects()

   if not sysbench.opt.skip_trx then
      prepare_begin()
      prepare_commit()
   end

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

   -- construct an time interval where sysbench don't need many buffer pool
   local current_time = os.time()
   local time_interval = current_time - start_time

   if time_interval <= 130 then
      if c_thread_id < 8 then
         execute_point_selects_with_table_id(c_thread_id+1)
         execute_simple_ranges_with_table_id(c_thread_id+1)
         execute_sum_ranges_with_table_id(c_thread_id+1)
         execute_order_ranges_with_table_id(c_thread_id+1)
         execute_point_selects_with_table_id(c_thread_id+1)
         execute_simple_ranges_with_table_id(c_thread_id+1)
         execute_sum_ranges_with_table_id(c_thread_id+1)
         execute_order_ranges_with_table_id(c_thread_id+1)
         execute_index_updates()
      else
         execute_point_selects_with_table_id((c_thread_id+1)%20+1)
         execute_simple_ranges_with_table_id((c_thread_id+1)%20+1)
         execute_sum_ranges_with_table_id((c_thread_id+1)%20+1)
         execute_order_ranges_with_table_id((c_thread_id+1)%20+1)
         execute_distinct_ranges_with_table_id((c_thread_id+1)%20+1)
      end
   else
      execute_point_selects_with_table_id_and_ranges(1, 1)
      execute_simple_ranges_with_table_id_and_ranges(1, 1)
      execute_sum_ranges_with_table_id_and_ranges(1, 1)
      execute_order_ranges_with_table_id_and_ranges(1, 1)
      execute_distinct_ranges_with_table_id_and_ranges(1, 1)
   end

   if not sysbench.opt.skip_trx then
      commit()
   end

   check_reconnect()
end
