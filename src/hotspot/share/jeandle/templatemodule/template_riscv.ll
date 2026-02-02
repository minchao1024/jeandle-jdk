; ==============================================================================
; Platform-Specific Implementation: riscv64
; ==============================================================================
define hotspotcc i64 @jeandle.get_stack_pointer() "lower-phase"="0" {
    %stack_pointer = call i64 @llvm.read_register.i64(metadata !{!"sp"})
    ret i64 %stack_pointer
}

; Try to release the monitor lock when the lock is inflated
define hotspotcc i1 @jeandle.try_release_monitor_lock(i64 %mark_word) "lower-phase"="0" {
entry:
  %monitor_ptr = inttoptr i64 %mark_word to ptr
  %recursions_offset_no_monitor_value = load i32, ptr @ObjectMonitor.recursions_offset_no_monitor_value
  %recursions_addr = getelementptr inbounds i8, ptr %monitor_ptr, i32 %recursions_offset_no_monitor_value
  %recursions = load atomic i64, ptr %recursions_addr unordered, align 8
  %is_recursive_monitor_unlock = icmp ne i64 %recursions, 0
  br i1 %is_recursive_monitor_unlock, label %decrease_recursions, label %check_for_waiters

decrease_recursions:
  %new_recursions = sub i64 %recursions, 1
  store atomic i64 %new_recursions, ptr %recursions_addr unordered, align 8
  br label %return_true

check_for_waiters:
  %cxq_offset_no_monitor_value = load i32, ptr @ObjectMonitor.cxq_offset_no_monitor_value
  %cxq_addr = getelementptr inbounds i8, ptr %monitor_ptr, i32 %cxq_offset_no_monitor_value
  %cxq = load atomic i64, ptr %cxq_addr unordered, align 8
  %EntryList_offset_no_monitor_value = load i32, ptr @ObjectMonitor.EntryList_offset_no_monitor_value
  %EntryList_addr = getelementptr inbounds i8, ptr %monitor_ptr, i32 %EntryList_offset_no_monitor_value
  %EntryList = load atomic i64, ptr %EntryList_addr unordered, align 8
  %is_cxq_null = icmp eq i64 %cxq, 0
  %is_EntryList_null = icmp eq i64 %EntryList, 0
  %has_no_waiters = and i1 %is_cxq_null, %is_EntryList_null
  br i1 %has_no_waiters, label %clear_monitor_owner, label %return_false

clear_monitor_owner:
  %owner_offset_no_monitor_value = load i32, ptr @ObjectMonitor.owner_offset_no_monitor_value
  %owner_addr = getelementptr inbounds i8, ptr %monitor_ptr, i32 %owner_offset_no_monitor_value
  store atomic volatile i64 0, ptr %owner_addr seq_cst, align 8
  br label %return_true

return_true:
  ret i1 true

return_false:
  ret i1 false
}
