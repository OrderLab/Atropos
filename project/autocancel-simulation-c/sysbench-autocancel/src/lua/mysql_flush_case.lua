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

   execute_point_selects()

   if sysbench.opt.range_selects then
      execute_simple_ranges()
      execute_sum_ranges()
      execute_order_ranges()
      execute_distinct_ranges()
   end

   if c_thread_id <= 30 then
    execute_index_updates()
    execute_non_index_updates()
    execute_delete_inserts()
   end

   if not sysbench.opt.skip_trx then
      commit()
   end

   check_reconnect()
end