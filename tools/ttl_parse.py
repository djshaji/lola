import argparse
import json
import sys
from decimal import Decimal
from pathlib import Path

from rdflib import Graph, Namespace
from rdflib.namespace import RDF


LV2 = Namespace("http://lv2plug.in/ns/lv2core#")
ATOM = Namespace("http://lv2plug.in/ns/ext/atom#")
DOAP = Namespace("http://usefulinc.com/ns/doap#")
FOAF = Namespace("http://xmlns.com/foaf/0.1/")
RDFS = Namespace("http://www.w3.org/2000/01/rdf-schema#")
PATCH = Namespace("http://lv2plug.in/ns/ext/patch#")
MOD = Namespace("http://moddevices.com/ns/mod#")


def _literal_value(node):
    if node is None:
        return None
    if hasattr(node, "toPython"):
        value = node.toPython()
        if isinstance(value, Decimal):
            if value == value.to_integral_value():
                return int(value)
            return float(value)
        return value
    return str(node)


def _load_graph(path):
    graph = Graph()
    source = Path(path)

    if source.is_dir():
        ttl_files = sorted(source.rglob("*.ttl"))
        if not ttl_files:
            raise FileNotFoundError(f"No TTL files found in directory: {path}")
        for ttl_file in ttl_files:
            graph.parse(ttl_file.as_posix(), format="turtle")
    else:
        graph.parse(source.as_posix(), format="turtle")

    return graph


def _port_direction(graph, port_node):
    if (port_node, RDF.type, LV2.InputPort) in graph:
        return "input"
    if (port_node, RDF.type, LV2.OutputPort) in graph:
        return "output"
    return "unknown"


def _port_type(graph, port_node):
    if (port_node, RDF.type, LV2.AudioPort) in graph:
        return "audio"
    if (port_node, RDF.type, LV2.ControlPort) in graph:
        return "control"
    if (port_node, RDF.type, ATOM.AtomPort) in graph:
        return "atom"
    if (port_node, RDF.type, LV2.EventPort) in graph:
        return "event"
    return "other"


def _maintainer_name(graph, plugin_uri):
    maintainer = graph.value(plugin_uri, DOAP.maintainer)
    if maintainer is None:
        return None
    return graph.value(maintainer, FOAF.name)


def _binary_value(node):
    value = _literal_value(node)
    if value is None:
        return ""

    value = str(value)
    if value.startswith("file://"):
        return Path(value[7:]).name
    return value


def _path_properties(graph, plugin_uri):
    properties = []
    for writable in graph.objects(plugin_uri, PATCH.writable):
        if (writable, RDFS.range, ATOM.Path) not in graph:
            continue

        file_types = _literal_value(graph.value(writable, MOD.fileTypes))
        property_info = {
            "uri": str(writable),
            "label": str(_literal_value(graph.value(writable, RDFS.label)) or ""),
            "fileTypes": str(file_types or ""),
        }
        properties.append({k: v for k, v in property_info.items() if v not in (None, "")})

    properties.sort(key=lambda item: item.get("uri", ""))
    return properties


def parse_lv2_ttl_to_json(path):
    graph = _load_graph(path)
    plugins_json = []

    for plugin_uri in graph.subjects(RDF.type, LV2.Plugin):
        plugin_data = {
            "uri": str(plugin_uri),
            "name": str(_literal_value(graph.value(plugin_uri, DOAP.name) or graph.value(plugin_uri, LV2.name)) or ""),
            "description": str(_literal_value(graph.value(plugin_uri, DOAP.description)) or ""),
            "author": str(_literal_value(_maintainer_name(graph, plugin_uri) or graph.value(plugin_uri, FOAF.name)) or ""),
            "binary": _binary_value(graph.value(plugin_uri, LV2.binary)),
            "ports": [],
            "pathProperties": _path_properties(graph, plugin_uri),
        }

        for port_node in graph.objects(plugin_uri, LV2.port):
            port_info = {
                "name": str(_literal_value(graph.value(port_node, LV2.name)) or ""),
                "symbol": str(_literal_value(graph.value(port_node, LV2.symbol)) or ""),
                "index": _literal_value(graph.value(port_node, LV2.index)),
                "direction": _port_direction(graph, port_node),
                "type": _port_type(graph, port_node),
                "minimum": _literal_value(graph.value(port_node, LV2.minimum)),
                "maximum": _literal_value(graph.value(port_node, LV2.maximum)),
                "default": _literal_value(graph.value(port_node, LV2.default)),
            }

            plugin_data["ports"].append({key: value for key, value in port_info.items() if value not in (None, "")})

        plugin_data["ports"].sort(key=lambda port: port.get("index", 0))
        plugins_json.append(plugin_data)

    plugins_json.sort(key=lambda plugin: plugin.get("uri", ""))
    return json.dumps(plugins_json, indent=4)


def main(argv=None):
    parser = argparse.ArgumentParser(description="Parse LV2 plugin TTL files into JSON.")
    parser.add_argument("path", help="Path to an LV2 plugin TTL file or bundle directory")
    args = parser.parse_args(argv)

    print(parse_lv2_ttl_to_json(args.path))


if __name__ == "__main__":
    main(sys.argv[1:])
