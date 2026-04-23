import gdb

from dataclasses import dataclass
from typing import Union

MAX_ADDR = 0xFFFFFFFF
T_LIST_NODE = gdb.lookup_type("list_node_t")
TP_LIST_NODE = T_LIST_NODE.pointer()
T_UINTPTR = gdb.lookup_type("uintptr_t")


def offsetof(type_or_name: str | gdb.Type, field_name: str) -> int:
    if type(type_or_name) is str:
        _type = gdb.lookup_type(type_or_name)
    else:
        assert type(type_or_name) is gdb.Type
        _type = type_or_name

    pointer_type = _type.pointer()

    zero_val = gdb.Value(0)
    zero_ptr = zero_val.cast(pointer_type)

    try:
        field_val = zero_ptr[field_name]
    except gdb.error as e:
        print(f"Internal error: type '{_type}' has no field '{field_name}'")
        raise e
    field_ptr = field_val.reference_value()

    return int(field_ptr.address)


@dataclass
class ListNode:
    addr: int
    prev_node: Union["ListNode", None]
    next_node: Union["ListNode", None]
    ptr_node: gdb.Value
    ptr_container: Union[gdb.Value, None] = None


def walk_list_nodes(
    addr: int, t_container: gdb.Type | None, node_field_name: str | None
) -> list[ListNode]:
    assert type(addr) is int
    assert (t_container is None and node_field_name is None) or (
        t_container is not None and node_field_name is not None
    )

    val_addr = gdb.Value(addr)

    # Walk forward
    fwd_nodes: list[ListNode] = []
    ptr_addr = val_addr.cast(TP_LIST_NODE)
    prev_node: ListNode | None = None
    while ptr_addr != 0:
        list_node = ptr_addr.dereference()
        node = ListNode(addr=int(ptr_addr), prev_node=prev_node, next_node=None, ptr_node=ptr_addr)
        if prev_node:
            prev_node.next_node = node
            if prev_node.addr != int(list_node["p_prev"]):
                print(
                    f"WARNING: node at 0x{node.addr:08x} has an invalid 'p_prev' value"
                    f" 0x{int(list_node["p_prev"]):08x}, it was reached from 0x{prev_node.addr:08x}"
                )
        prev_node = node
        ptr_addr = list_node["p_next"]
        fwd_nodes.append(node)

    # Walk backward
    back_nodes: list[ListNode] = []
    ptr_addr = val_addr.cast(TP_LIST_NODE)
    next_node: ListNode | None = None
    while ptr_addr != 0:
        list_node = ptr_addr.dereference()
        node = ListNode(addr=int(ptr_addr), prev_node=None, next_node=next_node, ptr_node=ptr_addr)
        if next_node:
            next_node.prev_node = node
            if next_node.addr != int(list_node["p_next"]):
                print(
                    f"WARNING: node at 0x{node.addr:08x} has an invalid 'p_next' value"
                    f" 0x{int(list_node["p_next"]):08x}, it was reached from 0x{next_node.addr:08x}"
                )
        next_node = node
        ptr_addr = list_node["p_prev"]
        back_nodes.insert(0, node)

    # The first node in 'fwd_nodes' has empty 'prev_node'. Fix that.
    if len(back_nodes) >= 2:
        fwd_nodes[0].prev_node = back_nodes[-1].prev_node
    nodes = back_nodes[:-1] + fwd_nodes

    # Walk through the nodes and cast them to the container type, if requested.
    if t_container:
        for node in nodes:
            offset_in_container = offsetof(t_container, node_field_name)
            container_addr = node.addr - offset_in_container
            ptr_container = gdb.Value(container_addr).cast(t_container.pointer())
            node.ptr_container = ptr_container

    return nodes


def dump_list_nodes(list_addr: int, nodes: list[ListNode]):
    if len(nodes) == 0:
        print(f"Empty list at 0x{list_addr:08x}")

    for i, node in enumerate(nodes):
        prev_addr = node.prev_node.addr if node.prev_node else 0
        next_addr = node.next_node.addr if node.next_node else 0
        print(f"Node {i} at 0x{node.addr:08x}: prev 0x{prev_addr:08x}, next 0x{next_addr:08x}")

        if node.ptr_container:
            container = node.ptr_container.dereference()
            print(f"Container {container.type}:")
            gdb.execute(f"print *({container.type}*)({node.ptr_container})")
            print()


class CmdY(gdb.Command):
    """Prefix for custom ytkernel GDB commands."""

    def __init__(self):
        super().__init__("y", gdb.COMMAND_USER, prefix=True)


class CmdYList(gdb.Command):
    """Prefix for linked-list-related commands."""

    def __init__(self):
        super().__init__("y list", gdb.COMMAND_USER, prefix=True)


class CmdYListDump(gdb.Command):
    """Prints the nodes of a linked list."""

    def __init__(self):
        self.name = "y list dump"
        super().__init__(self.name, gdb.COMMAND_USER)

    def complete(self, text, word):
        num_args = gdb.string_to_argv(text)
        if len(num_args) <= 1:
            return gdb.COMPLETE_EXPRESSION
        else:
            return gdb.COMPLETE_SYMBOL

    def invoke(self, gdb_args, from_tty):
        args = gdb.string_to_argv(gdb_args)
        if len(args) == 0:
            print(f"Usage: {self.name} <ADDR>")
            print("Dump the nodes of a list containing the node at ADDR.")
            return

        # Parse argument 1: list node address or symbol.
        arg_addr = args[0]
        addr_gdbval: gdb.Value = gdb.parse_and_eval(arg_addr)
        addr_gdbtype = addr_gdbval.type
        if addr_gdbtype.code == gdb.TYPE_CODE_INT:
            addr = int(addr_gdbval)
        elif addr_gdbtype == TP_LIST_NODE:
            addr = int(addr_gdbval)
        else:
            print(
                f"Error: argument '{arg_addr}' has type '{addr_gdbtype}', but is expected to be an"
                f" integer or '{TP_LIST_NODE}'"
            )
            return

        # Parse optional argument 2: list node container struct.
        t_container = None
        if len(args) > 1:
            arg_node_type = args[1]
            try:
                t_container = gdb.lookup_type(arg_node_type)
            except gdb.error:
                print(f"Error: argument '{arg_node_type}' must name a type")
                return

        # Parse optional argument 3: node field name in the container struct.
        node_field_name = None
        if len(args) > 2:
            node_field_name = args[2]
            try:
                t_container[node_field_name]
            except KeyError:
                print(f"Error: type '{t_container}' has no field '{node_field_name}'")
                return

        if addr > MAX_ADDR:
            print(f"Error: address 0x{addr:08x} is too large (> 0x{MAX_ADDR:08x})")
            return

        if t_container is not None and node_field_name is None:
            print('Node field name in the container struct is not specified, using "list_node"')
            node_field_name = "list_node"

        try:
            nodes = walk_list_nodes(addr, t_container, node_field_name)
        except gdb.error as e:
            print(f"Error: {e}")
            print(f"Failed to walk the nodes of the list at 0x{addr:08x}")
            return

        dump_list_nodes(addr, nodes)


CmdY()
CmdYList()
CmdYListDump()
