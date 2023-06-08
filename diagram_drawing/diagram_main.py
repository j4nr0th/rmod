from xml.etree import ElementTree as ET
from dataclasses import dataclass
from subprocess import call


@dataclass
class GraphNode:
    name: str
    children: list
    type: str


if __name__ == "__main__":
    tree = ET.parse("../input/chain_dependence_test.xml")
    root = tree.getroot()
    chain_roots = []
    chain_elements = []
    assert (root.tag == "rmod")
    chain_names = []
    node_lists = []
    for chain in root.iter("chain"):
        chain_elements.append([e for e in chain.iter("element")])
        chain_names.append(chain.find("name").text)
    print(chain_names)
    for i, chain_list in enumerate(chain_elements):
        root_found = False
        for j, element in enumerate(chain_list):
            #   Check if element has parents
            has_parents = False
            name = ""
            children = []
            etype = None
            for child in element:
                if child.tag == "parent":
                    has_parents = True
                    break
                if child.tag == "label":
                    name = child.text
                if child.tag == "child":
                    children.append(child.text)
                if child.tag == "type":
                    assert not etype
                    etype = child.text
            assert etype
            if not has_parents:
                assert not root_found
                chain_roots.append(GraphNode(name, children, etype))
                root_found = True
                chain_list.remove(element)
                break

        node_list = []
        for j, element in enumerate(chain_list):
            label = None
            children = []
            etype = None
            for child in element:
                if child.tag == "label":
                    assert not label
                    label = child.text

                if child.tag == "type":
                    assert not etype
                    etype = child.text

                if child.tag == "child":
                    children.append(child.text)
            assert label
            assert etype
            node_list.append(GraphNode(label, children, etype))


        for n in node_list:
            children = [no for no in filter(lambda node: node.name in n.children, node_list)]
            n.children = children

        current_root = chain_roots[-1]
        root_children = [no for no in filter(lambda node: node.name in current_root.children, node_list)]
        assert len(root_children) > 0
        current_root.children = root_children
        node_list.insert(0, current_root)
        node_lists.append(node_list)

    for i, croot in enumerate(chain_roots):
        name = chain_names[i]
        node_list = node_lists[i]
        name = "_".join(name.split())

        with open(f"{name}.dot", "w") as f_out:
            f_out.write(f"strict digraph {{\nnode [shape=box]\n")
            for n in node_list:
                for child in n.children:
                    f_out.write(f"\t\"{n.name}\n({n.type})\" -> \"{child.name}\n({child.type})\"\n")
            f_out.write(f"}}\n")
        call(["dot", "-Tsvg", f"{name}.dot"])
