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

function event()

   if not sysbench.opt.skip_trx then
      begin()
   end

   if c_thread_id < 3 then
      execute_point_selects_with_table_id(c_thread_id+1)
      execute_simple_ranges_with_table_id(c_thread_id+1)
      execute_sum_ranges_with_table_id(c_thread_id+1)
      execute_order_ranges_with_table_id(c_thread_id+1)
      execute_index_updates_with_table_id(c_thread_id+1)
   else
      execute_point_selects_with_table_id(c_thread_id+1)
      execute_simple_ranges_with_table_id(c_thread_id+1)
      execute_sum_ranges_with_table_id(c_thread_id+1)
      execute_order_ranges_with_table_id(c_thread_id+1)
      execute_distinct_ranges_with_table_id(c_thread_id+1)
   end

   if not sysbench.opt.skip_trx then
      commit()
   end

   check_reconnect()
end
