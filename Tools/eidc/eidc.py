# tools/generate_protocol.py
from pathlib import Path
from jinja2 import Environment, FileSystemLoader
import argparse
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field


VALID_TYPES = {'u8', 'u16', 'u32', 'i32', 'f32', 'bool', 'string'}


@dataclass
class FieldDef:
    name: str
    type: str


@dataclass
class ProcedureDef:
    name: str
    id: int
    request:  list[FieldDef] = field(default_factory=list)
    response: list[FieldDef] = field(default_factory=list)


def _parse_fields(block) -> list[FieldDef]:
    if block is None:
        return []
    fields = []
    for f in block.findall('Field'):
        name = f.get('name')
        type_ = f.get('type')
        if not name:
            raise ValueError(f"Field missing 'name' attribute")
        if type_ not in VALID_TYPES:
            raise ValueError(
                f"Field '{name}' has unknown type '{type_}'. "
                f"Valid types: {sorted(VALID_TYPES)}")
        fields.append(FieldDef(name=name, type=type_))
    return fields


def parse(path: str) -> list[ProcedureDef]:
    tree = ET.parse(path)
    root = tree.getroot()

    if root.tag != 'Protocol':
        raise ValueError(f"Root element must be <Protocol>, got <{root.tag}>")

    procedures = []
    seen_ids   = {}
    seen_names = set()

    for rpc in root.findall('Procedure'):
        name   = rpc.get('name')
        id_str = rpc.get('id')

        if not name:
            raise ValueError("<Procedure> missing 'name' attribute")
        if not id_str:
            raise ValueError(f"<Procedure name='{name}'> missing 'id' attribute")
        if name in seen_names:
            raise ValueError(f"Duplicate procedure name '{name}'")

        try:
            proc_id = int(id_str, 0)  # handles 0xFF and decimal
        except ValueError:
            raise ValueError(
                f"<Procedure name='{name}'> has invalid id '{id_str}' "
                f"— expected integer or 0x hex")

        if proc_id in seen_ids:
            raise ValueError(
                f"Duplicate procedure id {hex(proc_id)} "
                f"used by '{seen_ids[proc_id]}' and '{name}'")

        seen_ids[proc_id] = name
        seen_names.add(name)

        procedures.append(ProcedureDef(
            name     = name,
            id       = proc_id,
            request  = _parse_fields(rpc.find('Request')),
            response = _parse_fields(rpc.find('Response')),
        ))

    return procedures

# ── Type tables ──────────────────────────────────────────────────────────────

PY_STRUCT_CHAR = {
    'u8': 'B', 'u16': 'H', 'u32': 'I',
    'i32': 'i', 'f32': 'f', 'bool': 'B',
}

PY_SIZE = {
    'u8': 1, 'u16': 2, 'u32': 4,
    'i32': 4, 'f32': 4, 'bool': 1,
}

PY_ANNOTATION = {
    'u8': 'int', 'u16': 'int', 'u32': 'int',
    'i32': 'int', 'f32': 'float', 'bool': 'bool', 'string': 'str',
}

PY_DEFAULT = {
    'u8': '0', 'u16': '0', 'u32': '0',
    'i32': '0', 'f32': '0.0', 'bool': 'False', 'string': "''",
}

CPP_TYPE = {
    'u8': 'uint8_t', 'u16': 'uint16_t', 'u32': 'uint32_t',
    'i32': 'int32_t', 'f32': 'float', 'bool': 'bool', 'string': 'std::string',
}

CPP_DEFAULT = {
    'u8': '0', 'u16': '0', 'u32': '0',
    'i32': '0', 'f32': '0.0f', 'bool': 'false', 'string': '{}',
}

CPP_WRITE = {
    'u8': 'WriteU8', 'u16': 'WriteU16', 'u32': 'WriteU32',
    'i32': 'WriteI32', 'f32': 'WriteFloat', 'string': 'WriteString',
}

CPP_READ = {
    'u8': 'ReadU8', 'u16': 'ReadU16', 'u32': 'ReadU32',
    'i32': 'ReadI32', 'f32': 'ReadFloat', 'string': 'ReadString',
}


# ── Jinja filters ────────────────────────────────────────────────────────────

def filter_hex(value: int) -> str:
    return hex(value)

def filter_py_struct_char(type_: str) -> str:
    return PY_STRUCT_CHAR[type_]

def filter_py_size(type_: str) -> int:
    return PY_SIZE[type_]

def filter_py_fmt(fields: list[FieldDef]) -> str:
    return '<' + ''.join(PY_STRUCT_CHAR[f.type] for f in fields if f.type != 'string')

def filter_py_params(fields: list[FieldDef]) -> str:
    return ', '.join(
        f"{f.name}: {PY_ANNOTATION[f.type]} = {PY_DEFAULT[f.type]}"
        for f in fields)

def filter_py_pack_args(fields: list[FieldDef]) -> str:
    return ', '.join(
        f"int(self.{f.name})" if f.type == 'bool' else f"self.{f.name}"
        for f in fields)

def filter_py_field_names(fields: list[FieldDef]) -> str:
    return ', '.join(f.name for f in fields)

def filter_cpp_type(type_: str) -> str:
    return CPP_TYPE[type_]

def filter_cpp_default(type_: str) -> str:
    return CPP_DEFAULT[type_]

def filter_cpp_write(type_: str) -> str:
    return CPP_WRITE[type_]

def filter_cpp_read(type_: str) -> str:
    return CPP_READ[type_]

def filter_camel(snake: str) -> str:
    parts = snake.split('_')
    return parts[0] + ''.join(p.capitalize() for p in parts[1:])


# ── Jinja environment ────────────────────────────────────────────────────────

def build_env(templates_dir: Path) -> Environment:
    env = Environment(
        loader=FileSystemLoader(str(templates_dir)),
        trim_blocks=True,
        lstrip_blocks=True,
    )
    env.filters.update({
        'hex':            filter_hex,
        'py_struct_char': filter_py_struct_char,
        'py_size':        filter_py_size,
        'py_fmt':         filter_py_fmt,
        'py_params':      filter_py_params,
        'py_pack_args':   filter_py_pack_args,
        'py_field_names': filter_py_field_names,
        'cpp_type':       filter_cpp_type,
        'cpp_default':    filter_cpp_default,
        'cpp_write':      filter_cpp_write,
        'cpp_read':       filter_cpp_read,
        'camel':          filter_camel,
    })
    return env


# ── Targets ──────────────────────────────────────────────────────────────────

# Maps target name → (template filename, default output path relative to repo root)
TARGETS = {
    'python': ('py.j2',  'Tools/elysium/elysium/generated.py'),
    'cpp':    ('cpp.j2', 'Source/Network/Generated.h'),
}


# ── Argument parser ──────────────────────────────────────────────────────────

def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog='generate_protocol',
        description='Generate C++ and Python bindings from an Elysium invoke.xml descriptor.',
    )

    parser.add_argument(
        'idl',
        metavar='IDL',
        type=Path,
        help='Path to invoke.xml',
    )

    parser.add_argument(
        '--targets',
        nargs='+',
        metavar='TARGET',
        choices=TARGETS.keys(),
        default=list(TARGETS.keys()),
        help=f'Which targets to generate. Choices: {", ".join(TARGETS)}. Default: all.',
    )

    parser.add_argument(
        '--templates',
        metavar='DIR',
        type=Path,
        default=Path(__file__).parent / 'templates',
        help='Path to Jinja2 templates directory. Default: tools/templates/',
    )

    parser.add_argument(
        '--out-python',
        metavar='FILE',
        type=Path,
        default=None,
        help='Output path for generated Python file. Default: elysium/generated.py',
    )

    parser.add_argument(
        '--out-cpp',
        metavar='FILE',
        type=Path,
        default=None,
        help='Output path for generated C++ header. Default: Source/Network/Generated.h',
    )

    parser.add_argument(
        '--dry-run',
        action='store_true',
        help='Parse and render but do not write any files.',
    )

    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Print parsed procedure details.',
    )

    return parser


# ── Main ─────────────────────────────────────────────────────────────────────

def main():
    parser = build_parser()
    args   = parser.parse_args()

    # Validate IDL path
    if not args.idl.exists():
        print(f"error: IDL file not found: {args.idl}", file=sys.stderr)
        sys.exit(1)

    if not args.templates.is_dir():
        print(f"error: templates directory not found: {args.templates}", file=sys.stderr)
        sys.exit(1)

    # Parse
    try:
        procs = parse(str(args.idl))
    except Exception as e:
        print(f"error: failed to parse {args.idl}: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"Parsed {len(procs)} procedure(s) from {args.idl}")

    if args.verbose:
        for p in procs:
            print(f"  {p.name} ({hex(p.id)}) — "
                  f"{len(p.request)} request field(s), "
                  f"{len(p.response)} response field(s)")

    # Resolve output paths — CLI overrides take priority, then defaults
    repo_root = Path(__file__).parent.parent.parent
    out_paths = {
        'python': args.out_python or repo_root / TARGETS['python'][1],
        'cpp':    args.out_cpp    or repo_root / TARGETS['cpp'][1],
    }

    env     = build_env(args.templates)
    context = {'procedures': procs}

    for target in args.targets:
        template_name, _ = TARGETS[target]
        out_path          = out_paths[target]

        try:
            rendered = env.get_template(template_name).render(context)
        except Exception as e:
            print(f"error: failed to render {template_name}: {e}", file=sys.stderr)
            sys.exit(1)

        if args.dry_run:
            print(f"[dry-run] would write {out_path} ({len(rendered)} chars)")
        else:
            out_path.parent.mkdir(parents=True, exist_ok=True)
            out_path.write_text(rendered)
            print(f"Generated {out_path}")


if __name__ == '__main__':
    main()