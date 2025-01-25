    PUBLIC _spoofer_stub
     
    .code
     
    _spoofer_stub PROC
        pop r11 ; poping without setting up stack frame, r11 is the return address (the one in our code)
        add rsp, 8 ; skipping callee reserved space
        mov rax, [rsp + 24] ; dereference shell_param
        
        mov r10, [rax] ; load shell_param.trampoline
        mov [rsp], r10 ; store address of trampoline as return address
        
        mov r10, [rax + 8] ; load shell_param.function
        mov [rax + 8], r11 ; store the original return address in shell_param.function
     
        mov [rax + 16], rdi ; preserve rdi in shell_param.rdi
        lea rdi, fixup
        mov [rax], rdi ; store address of fixup label in shell_param.trampoline
        mov rdi, rax ; preserve address of shell_param in rdi
        
        jmp r10 ; call shell_param.function
     
    fixup:
        sub rsp, 16
        mov rcx, rdi ; restore address of shell_param
        mov rdi, [rcx + 16] ; restore rdi from shell_param.rdi
        jmp QWORD PTR [rcx + 8] ; jmp to the original return address
    _spoofer_stub ENDP
     
    END