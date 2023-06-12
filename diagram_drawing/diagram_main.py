from xml.etree import ElementTree as Et
from dataclasses import dataclass
from subprocess import call


@dataclass
class GraphNode:
    label: str
    type: str
    # MTBF: str
    child_array: list


if __name__ == "__main__":
    tree = Et.parse("../input/cool.xml")
    root = tree.getroot()
    chain_elements = []
    assert (root.tag == "rmod")
    chain_names = []
    node_lists = []
    for chain in root.iter("chain"):
        chain_elements.append([e for e in chain.iter("element")])
        chain_names.append(chain.find("name").text)
    print(chain_names)
    for i, chain_list in enumerate(chain_elements):
        n_list = []
        for j, ce in enumerate(chain_list):
            label = None
            etype = None
            # MTBF = None
            child_array = []
            for prop in ce:
                if prop.tag == "label":
                    assert not label
                    label = prop.text
                elif prop.tag == "type":
                    assert not etype
                    etype = prop.text
                # elif prop.tag == "mtbf":
                #     assert not MTBF
                #     MTBF = prop.text
                elif prop.tag == "child":
                    child_array.append(prop.text)
            assert label
            assert etype
            # assert MTBF
            n_list.append(GraphNode(label, etype, child_array))
        node_lists.append(n_list)

    for i, node_list in enumerate(node_lists):
        name = chain_names[i]
        name = "_".join(name.split())

        with open(f"{name}.dot", "w") as f_out:
            f_out.write(f"strict digraph {{\nnode [shape=box]\n")
            for n in node_list:
                for child in n.child_array:
                    f_out.write(f"\t\"{n.label}\" [label=\"{n.type}\"]\n")
                    f_out.write(f"\t\"{n.label}\" -> \"{child}\"\n")
            f_out.write(f"}}\n")
        call(["dot", "-Tsvg", f"{name}.dot"])
