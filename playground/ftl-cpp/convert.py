import xml.etree.ElementTree as ET

XML_START = b"<?xml"
XML_END = b"</FTL>"

def parse_xmls(data: bytes, start: int = 0):
    pos = start

    while True:
        begin = data.find(XML_START, pos)
        if begin == -1:
            break

        end = data.find(XML_END, begin)
        if end == -1:
            break

        end += len(XML_END)
        xml_bytes = data[begin:end]

        try:
            root = ET.fromstring(xml_bytes)  # Pass bytes directly
            yield begin, end, root
            pos = end
        except ET.ParseError as e:
            print(f"Parse error at byte {begin}: {e}")
            break

def open_file(filename):
    with open(filename, "rb") as f:
        return f.read()

if __name__ == "__main__":
    
    filename = "ftl.dat"
    data = open_file(filename)

    for begin, end, root in parse_xmls(data):
        print(f"Found XML document from {begin} to {end}")
        print(ET.tostring(root, encoding="unicode"))
