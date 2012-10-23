#!/usr/bin/env ruby
#
# This file is gererated by ruby-glade-create-template 1.1.3.
#
require 'libglade2'
require 'sta-viewer_glade'

class StaViewer < StaViewerGlade
  def initialize(path_or_data, root = nil, domain = nil, localedir = nil, flag = GladeXML::FILE)
    bindtextdomain(domain, localedir, nil, "UTF-8")
    super(path_or_data, root = nil, domain = nil, localedir = nil, flag = GladeXML::FILE)
    
  end
  
  def on_window1_destroy_event(widget, arg0)
    Gtk.main_quit
  end
  def on_toolbutton1_clicked(widget)
    Gtk.main_quit
  end
end

# Main program
if __FILE__ == $0
  # Set values as your own application. 
  PROG_PATH = "sta-viewer.glade"
  PROG_NAME = "STA Viewer"
  Gtk.init
  StaViewer.new(PROG_PATH, nil, PROG_NAME)
  Gtk.main
end
