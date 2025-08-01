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

   prepare_all_selects()
end

start_time = nil
interval = 150

function event()

   if not start_time then
      start_time = os.time()
   end

   local current_time = os.time()
   local time_interval = current_time - start_time

   if time_interval <= interval then
      execute_index_updates_with_table_id_and_row_id(1, c_thread_id+1)
   else
      os.execute("sleep " .. 300 - interval)
   end
   --[[
   if (c_thread_id == 0) then
      execute_index_updates_with_table_id_and_row_id(1, 1)
   elseif (c_thread_id == 1) then
      execute_index_updates_with_table_id_and_row_id(1, 2)
   else
      begin()

      execute_point_selects()
      execute_simple_ranges()
      execute_sum_ranges()
      execute_order_ranges()
      execute_distinct_ranges()

      execute_index_updates()

      commit()
   end
   --]]

   check_reconnect()
end