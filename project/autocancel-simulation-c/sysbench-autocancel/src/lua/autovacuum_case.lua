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

function event()

   if not sysbench.opt.skip_trx then
      begin()
   end
--[[ 
   execute_point_selects()
   if c_thread_id < 8 then
      execute_point_selects_with_table_id(21)
      execute_simple_ranges_with_table_id(21)
      execute_sum_ranges_with_table_id(21)
      execute_order_ranges_with_table_id(21)
      execute_distinct_ranges_with_table_id(21)
      execute_point_selects_with_table_id(21)
      execute_simple_ranges_with_table_id(21)
      execute_sum_ranges_with_table_id(21)
      execute_order_ranges_with_table_id(21)
      execute_distinct_ranges_with_table_id(21)
   end

   if sysbench.opt.range_selects then
      execute_simple_ranges()
      execute_sum_ranges()
      execute_order_ranges()
      execute_distinct_ranges()
   end

   execute_index_updates()
   execute_non_index_updates()
   execute_delete_inserts()
]]
   if c_thread_id < 9 then
      execute_point_selects_under_table_id(20)
      execute_sum_ranges_under_table_id(20)
      execute_order_ranges_under_table_id(20)
      execute_distinct_ranges_under_table_id(20)
      execute_simple_ranges_under_table_id(20)
      execute_index_updates_under_table_id(20)
      execute_delete_inserts_under_table_id(20)
   else
      execute_point_selects_with_table_id_and_ranges(21, 1000000)
      execute_sum_ranges_with_table_id_and_ranges(21, 1000000)
      execute_order_ranges_with_table_id_and_ranges(21, 1000000)
      execute_distinct_ranges_with_table_id_and_ranges(21, 1000000)
      execute_simple_ranges_with_table_id_and_ranges(21, 1000000)
      execute_index_updates_with_table_id_and_range(21, 10000000)
      execute_delete_inserts_with_table_id_and_ranges(21, 10000000)
   end

   if not sysbench.opt.skip_trx then
      commit()
   end

   check_reconnect()
end
