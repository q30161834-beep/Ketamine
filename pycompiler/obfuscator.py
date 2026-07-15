"""Code obfuscator - makes reverse engineering harder"""
import random
import string
import hashlib

class CodeObfuscator:
    def __init__(self):
        self.name_map = {}
        self.counter = 0
    
    def obfuscate(self, code: str, target: str = "js") -> str:
        """Apply obfuscation techniques"""
        if target == "js":
            return self._obfuscate_js(code)
        elif target == "wasm":
            return code  # WASM is already binary
        else:
            return self._obfuscate_llvm(code)
    
    def _obfuscate_js(self, code: str) -> str:
        """JavaScript-specific obfuscation"""
        # 1. Rename variables/functions
        code = self._rename_identifiers(code)
        
        # 2. Insert dead code
        code = self._insert_dead_code_js(code)
        
        # 3. String encoding
        code = self._encode_strings_js(code)
        
        # 4. Control flow flattening (simplified)
        code = self._flatten_control_flow_js(code)
        
        return code
    
    def _rename_identifiers(self, code: str) -> str:
        """Rename all user-defined identifiers to obfuscated names"""
        import re
        
        # Find all identifiers (simplified regex)
        identifiers = set(re.findall(r'\b([a-zA-Z_]\w*)\b', code))
        
        # Exclude keywords and builtins
        keywords = {'function', 'const', 'let', 'var', 'if', 'else', 'while',
                    'for', 'return', 'console', 'log', 'String', 'Math',
                    'process', 'exit', 'true', 'false', 'null', 'undefined'}
        
        identifiers -= keywords
        
        # Generate obfuscated names
        for ident in identifiers:
            if ident not in self.name_map:
                self.name_map[ident] = self._gen_obfuscated_name()
        
        # Replace (whole word only)
        for orig, obf in self.name_map.items():
            code = re.sub(r'\b' + re.escape(orig) + r'\b', obf, code)
        
        return code
    
    def _gen_obfuscated_name(self) -> str:
        """Generate an obfuscated identifier"""
        # Strategy: use Unicode confusables or random hex
        self.counter += 1
        
        # Method 1: hex names
        return f"_{hex(self.counter)[2:]}_{random.randint(0, 999):03x}"
        
        # Method 2: Unicode confusables (advanced)
        # confusables = ['Ӏ', 'І', 'Ι', 'Ӏ']  # looks like 'I'
        # return ''.join(random.choices(confusables, k=5))
    
    def _insert_dead_code_js(self, code: str) -> str:
        """Insert dead code that never executes"""
        dead_snippets = [
            "if (false) { const _dead = Math.random(); }",
            "const _unused = 0;",
            "// " + ''.join(random.choices(string.ascii_letters, k=20)),
            "void 0;",
        ]
        
        lines = code.split('\n')
        for i in range(len(lines)):
            if random.random() < 0.1:  # 10% chance
                lines[i] += "  " + random.choice(dead_snippets)
        
        return '\n'.join(lines)
    
    def _encode_strings_js(self, code: str) -> str:
        """Encode string literals"""
        import re
        
        def encode_match(match):
            s = match.group(1)
            # Base64 encode
            import base64
            encoded = base64.b64encode(s.encode()).decode()
            return f'atob("{encoded}")'
        
        # Replace string literals (simplified)
        code = re.sub(r'"([^"]*)"', encode_match, code)
        
        return code
    
    def _flatten_control_flow_js(self, code: str) -> str:
        """Simple control flow obfuscation"""
        # Insert opaque predicates
        predicates = [
            "(Math.random() < 2)",  # always true
            "(1 === 1)",
            "(!false)",
        ]
        
        # Wrap some blocks in opaque predicates
        lines = code.split('\n')
        result = []
        
        for line in lines:
            if 'if' in line and random.random() < 0.2:
                pred = random.choice(predicates)
                result.append(f"if ({pred}) {{")
                result.append("  " + line)
                result.append("}")
            else:
                result.append(line)
        
        return '\n'.join(result)
    
    def _obfuscate_llvm(self, code: str) -> str:
        """LLVM IR obfuscation"""
        # Rename SSA registers
        import re
        
        registers = set(re.findall(r'%t(\d+)', code))
        reg_map = {}
        
        for reg in registers:
            new_reg = random.randint(1000, 9999)
            reg_map[f'%t{reg}'] = f'%t{new_reg}'
        
        for old, new in reg_map.items():
            code = code.replace(old, new)
        
        # Insert nop instructions
        lines = code.split('\n')
        result = []
        for line in lines:
            result.append(line)
            if random.random() < 0.1 and '=' in line:
                # Insert dead computation
                tmp = random.randint(10000, 99999)
                result.append(f"  %dead{tmp} = add i64 0, 0")
        
        return '\n'.join(result)

class AntiTampering:
    """Detect code modification at runtime"""
    
    @staticmethod
    def checksum(code: str) -> str:
        """Calculate code checksum"""
        return hashlib.sha256(code.encode()).hexdigest()
    
    @staticmethod
    def generate_check(code: str) -> str:
        """Generate self-checking code"""
        checksum = AntiTampering.checksum(code)
        
        check_code = f'''
// Anti-tampering check
(function() {{
    const expected = "{checksum}";
    const actual = require('crypto').createHash('sha256')
        .update(String(arguments.callee))
        .digest('hex');
    if (actual !== expected) {{
        throw new Error("Code integrity violation!");
    }}
}})();
'''
        return check_code + "\n" + code

class PolyEngine:
    """Polymorphic code engine - generates different code each time"""
    
    @staticmethod
    def polymorphize(code: str, seed: int = None) -> str:
        """Generate polymorphic variant"""
        if seed:
            random.seed(seed)
        
        # Add random junk instructions
        junk = [
            f"const _junk{i} = {random.randint(0, 1000)};"
            for i in range(random.randint(5, 15))
        ]
        
        # Insert at random positions
        lines = code.split('\n')
        for j in junk:
            pos = random.randint(0, len(lines))
            lines.insert(pos, j)
        
        return '\n'.join(lines)
