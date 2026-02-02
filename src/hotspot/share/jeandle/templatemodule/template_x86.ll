; ==============================================================================
; Platform-Specific Implementation: x86_64 (AMD64)
; ==============================================================================
define hotspotcc i64 @jeandle.get_stack_pointer() "lower-phase"="0" {
    %stack_pointer = call i64 @llvm.read_register.i64(metadata !{!"rsp"})
    ret i64 %stack_pointer
}
