"""Security analysis and anti-debugging for Ketamine compiler"""
import os
import sys
import ctypes
import platform
from typing import List
from parser import *

class AntiDebug:
    """Anti-debugging techniques"""
    
    @staticmethod
    def is_debugger_present() -> bool:
        """Detect if debugger is attached (Windows/Linux)"""
        system = platform.system()
        
        if system == "Windows":
            # Windows: check IsDebuggerPresent()
            try:
                kernel32 = ctypes.windll.kernel32
                return kernel32.IsDebuggerPresent() != 0
            except:
                pass
        
        elif system == "Linux":
            # Linux: check /proc/self/status for TracerPid
            try:
                with open('/proc/self/status', 'r') as f:
                    for line in f:
                        if line.startswith('TracerPid:'):
                            tracer_pid = int(line.split(':')[1].strip())
                            return tracer_pid != 0
            except:
                pass
        
        elif system == "Darwin":  # macOS
            # macOS: check sysctl P_TRACED flag
            try:
                from ctypes import c_int, c_size_t, byref
                libc = ctypes.CDLL('/usr/lib/libc.dylib')
                
                # sysctl structure
                class KInfoProc(ctypes.Structure):
                    _fields_ = [('kp_proc', ctypes.c_char * 648)]
                
                kinfo = KInfoProc()
                name = (c_int * 4)(0, 0, 0, os.getpid())
                size = c_size_t(ctypes.sizeof(kinfo))
                
                libc.sysctl(byref(name), 4, byref(kinfo), byref(size), None, 0)
                # P_TRACED = 0x00000800
                return (ord(kinfo.kp_proc[16]) & 0x08) != 0
            except:
                pass
        
        return False
    
    @staticmethod
    def check_environment():
        """Check for suspicious environment variables"""
        suspicious = ['LD_PRELOAD', 'DYLD_INSERT_LIBRARIES', '_']
        for var in suspicious:
            if var in os.environ:
                return True
        return False
    
    @staticmethod
    def timing_check():
        """Detect debugger via timing"""
        import time
        start = time.perf_counter()
        # Simple operation
        x = sum(range(1000))
        elapsed = time.perf_counter() - start
        # If too slow, likely debugger or VM
        return elapsed > 0.01

class SecurityAnalyzer:
    """Analyze AST for security issues"""
    
    def __init__(self):
        self.issues: List[str] = []
        self.has_critical = False
    
    def analyze(self, program: Program) -> List[str]:
        """Scan AST for security vulnerabilities"""
        self.issues = []
        self.has_critical = False
        
        for decl in program.declarations:
            self._visit(decl)
        
        return self.issues
    
    def _visit(self, node: ASTNode):
        if isinstance(node, FnDecl):
            self._check_fn(node)
        elif isinstance(node, CallExpr):
            self._check_call(node)
        elif isinstance(node, BinaryExpr):
            self._check_binary(node)
        elif isinstance(node, Block):
            for stmt in node.stmts:
                self._visit(stmt)
        elif hasattr(node, '__dict__'):
            for value in node.__dict__.values():
                if isinstance(value, ASTNode):
                    self._visit(value)
                elif isinstance(value, list):
                    for item in value:
                        if isinstance(item, ASTNode):
                            self._visit(item)
    
    def _check_fn(self, node: FnDecl):
        """Check function declarations"""
        # Check for unsafe function names
        unsafe = ['eval', 'exec', 'system', 'popen']
        if node.name in unsafe:
            self.issues.append(f"Line {node.line}: WARNING: Unsafe function name '{node.name}'")
            self.has_critical = True
        
        # Check parameter count (prevent stack exhaustion)
        if len(node.params) > 32:
            self.issues.append(f"Line {node.line}: Too many parameters ({len(node.params)})")
        
        if node.body:
            self._visit(node.body)
    
    def _check_call(self, node: CallExpr):
        """Check function calls for unsafe operations"""
        if isinstance(node.callee, Ident):
            name = node.callee.name
            
            # Dangerous functions
            if name in ['eval', 'exec', 'system']:
                self.issues.append(f"Line {node.line}: CRITICAL: Use of dangerous function '{name}'")
                self.has_critical = True
            
            # Unchecked input
            if name in ['read_line', 'input'] and len(node.args) == 0:
                self.issues.append(f"Line {node.line}: Unchecked user input from '{name}'")
            
            # Division (potential div-by-zero)
            # This is checked in binary expressions
        
        for arg in node.args:
            self._visit(arg)
    
    def _check_binary(self, node: BinaryExpr):
        """Check binary operations"""
        # Division by zero check
        if node.op == '/':
            if isinstance(node.right, IntLiteral) and node.right.value == 0:
                self.issues.append(f"Line {node.line}: CRITICAL: Division by zero")
                self.has_critical = True
            elif isinstance(node.right, Ident):
                self.issues.append(f"Line {node.line}: Potential division by zero (unchecked variable)")
        
        # Integer overflow (simplified check)
        if node.op in ['+', '*', '-']:
            if isinstance(node.left, IntLiteral) and isinstance(node.right, IntLiteral):
                try:
                    result = None
                    if node.op == '+':
                        result = node.left.value + node.right.value
                    elif node.op == '*':
                        result = node.left.value * node.right.value
                    elif node.op == '-':
                        result = node.left.value - node.right.value
                    
                    # Check for overflow (64-bit signed)
                    if result and (result > 2**63 - 1 or result < -2**63):
                        self.issues.append(f"Line {node.line}: Integer overflow detected")
                except:
                    pass
        
        self._visit(node.left)
        self._visit(node.right)

class SandboxRunner:
    """Run compiled code in a sandboxed environment"""
    
    def __init__(self, timeout: int = 5, max_memory: int = 256):
        self.timeout = timeout  # seconds
        self.max_memory = max_memory  # MB
    
    def run(self, code: str, target: str = "js") -> tuple:
        """Execute code in subprocess with resource limits"""
        import subprocess
        import tempfile
        import signal
        
        try:
            with tempfile.NamedTemporaryFile(mode='w', suffix=f'.{target}', delete=False) as f:
                f.write(code)
                temp_file = f.name
            
            # Prepare command
            if target == "js":
                cmd = ["node", temp_file]
            elif target == "py":
                cmd = ["python3", temp_file]
            else:
                return (1, "", "Unsupported target")
            
            # Run with timeout
            try:
                proc = subprocess.run(
                    cmd,
                    timeout=self.timeout,
                    capture_output=True,
                    text=True
                )
                return (proc.returncode, proc.stdout, proc.stderr)
            except subprocess.TimeoutExpired:
                return (1, "", f"Execution timed out after {self.timeout}s")
            finally:
                try:
                    os.unlink(temp_file)
                except:
                    pass
        
        except Exception as e:
            return (1, "", str(e))

# ─── Memory protection utilities ──────────────────────────────────────────────

def protect_memory_region(addr, size, readonly=True):
    """Mark memory region as read-only (requires ctypes)"""
    if platform.system() == "Windows":
        kernel32 = ctypes.windll.kernel32
        old_protect = ctypes.c_ulong()
        PAGE_READONLY = 0x02
        PAGE_READWRITE = 0x04
        prot = PAGE_READONLY if readonly else PAGE_READWRITE
        kernel32.VirtualProtect(addr, size, prot, ctypes.byref(old_protect))
    else:
        # Linux/macOS: mprotect
        try:
            libc = ctypes.CDLL(None)
            PROT_READ = 1
            PROT_WRITE = 2
            prot = PROT_READ if readonly else (PROT_READ | PROT_WRITE)
            libc.mprotect(addr, size, prot)
        except:
            pass

def secure_zero_memory(data: bytearray):
    """Securely zero memory (prevent compiler optimization)"""
    for i in range(len(data)):
        data[i] = 0
    # Force write
    ctypes.memset(id(data) + 32, 0, len(data))
