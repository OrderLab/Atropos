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
end

function event()

   if not sysbench.opt.skip_trx then
      begin()
   end

   if c_thread_id == 0 then
      execute_point_selects_with_table_id(3)
   else
      execute_point_selects_with_table_id(1)
   end

   if sysbench.opt.range_selects then
      if c_thread_id == 2 then
        execute_simple_ranges_with_table_id(3)
        execute_sum_ranges_with_table_id(3)
        execute_order_ranges_with_table_id(3)
        execute_distinct_ranges_with_table_id(3)
      else
        execute_simple_ranges_with_table_id(1)
        execute_sum_ranges_with_table_id(1)
        execute_order_ranges_with_table_id(1)
        execute_distinct_ranges_with_table_id(1)
      end
   end

   if not sysbench.opt.skip_trx then
      commit()
   end

   check_reconnect()
end
