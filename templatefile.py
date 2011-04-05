from string import Template
import os

class TemplateFile:
    """A template for creating a source file"""

    def __init__(self, path):
        # default values...
        # no name or file yet
        self.name = ''
        self.sections = {}
        self.file = None
        self.path = None
        # open a new file for each item
        self.file_per_item = False

        # empty footer if none specified:
        self.sections['footer'] = []
        # empty per-port if none specified:
        self.sections['port'] = []

        # lines appended to body by default
        self.sections['body'] = []
        current = self.sections['body']

        self.template = open(path)
        for line in self.template:
            line = line.rstrip()
            if line.startswith('@'):
                if ' ' in line:
                    leading, trailing = line.split(' ', 1)
                else:
                    leading, trailing = line, None

                if leading == '@name':
                    if not trailing:
                        raise Exception("@name requires a file name.")
                    self.path = trailing
                    if '$' in self.path:
                        self.file_per_item = True
                else:
                    section = leading[1:]
                    if section not in self.sections:
                        self.sections[section] = []
                    current = self.sections[section]
            else:
                current.append(line)
        self.template.close()
        for section, data in self.sections.items():
            if len(data) > 0:
                self.sections[section] = Template("\n".join(data))
            else:
                self.sections[section] = None

        # You need a file if this isn't a file-per-item
        if not self.file_per_item:
            self.file = open(self.path, 'w')

    def close(self):
        """Close the associated file."""
        if self.file:
            self.file.close()
            self.file = None

    def __repr__(self):
        strings = []
        if self.file_per_item:
            strings.append("path: %s (per item)" % self.path)
        else:
            strings.append("path: %s" % self.path)
        for name, data in self.sections.items():
            strings.append("%s:" % name)
            strings.append(data.safe_substitute({}))
        return "\n".join(strings)

    def get_file(self, item):
        if self.file_per_item:
            if not item:
                return
            path = Template(self.path).safe_substitute(item)
            if os.path.exists(path):
                # print "We don't overwrite existing files."
                return
            self.file = open(path, 'w')
            if not self.file:
                print "Couldn't open '%s' (expanded from %s), " \
                      "not emitting '%s'." % \
                      (path, self.path, template)
                return

    def emit(self, template, item=None):
        """Emit a template, with optional interpolation of an item."""
        if template == "copyright":
            # hey, at least it's not a global variable, amirite?
            self.get_file(item)
            if self.file:
                self.file.write(TemplateFile.copyright)
        elif template in self.sections:
            templ = self.sections[template]
            if templ:
                self.get_file(item)
                if self.file:
                    self.file.write(templ.safe_substitute(item))
                    self.file.write("\n")
        else:
            print "Warning: Unknown template '%s'." % template

        if self.file_per_item:
            if self.file:
                self.file.close()
            self.file = None
