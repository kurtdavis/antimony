Scripting in Antimony
=====================

Every Antimony node is implemented as a Python script.

Scripts are Python 3.x with a few extra functions and modules.

I/O functions
-------------
- `input(name, type)` creates an input datum of the given type
and injects it into the namespace.  `name` must be a string
and `type` must be either `float`, `int`, or `fab.types.Shape`
(or simply `Shape` depending on how it was imported).
- `output(name, value)` creates an output datum with the given name and value
(the type is automatically determined and must be either `float` or `Shape`)
- `title(value)` sets the title of the node,
which is shown in a graph window in the upper-right corner of the node.

Note that `input` and `output` will create new rows in the graph view
for the datums that they add to the node.

`fab` module
------------
Python scripts will usually import the `fab` module, which has
Antimony-specific classes and types for designing shapes.

### `fab.shapes`
`fab.shapes` contains a standard library of primitives and transforms.
It is populated from [shapes.py](../py/fab/shapes.py).

### `fab.types`
`fab.types` contains Python classes representing various Antimony types.
The most relevant is `fab.types.Shape`, which represents a 2D or 3D
solid body.  It is often used as the second argument to `input`, i.e.
`input(s, fab.types.Shape)` to declare a `Shape` input port.

Other types include `Bounds` (representing 2D or 3D bounds) and
`Transform` (representing a coordinate transform).

### `fab.ui`
`fab.ui` contains magic functions that modify the user interface in
3D viewports.  The relevant functions are `fab.ui.wireframe` and
`fab.ui.point`.  Both have detailed docstrings and are used extensively
in the standard node library; check out existing nodes to get an idea
of how they can be used.

`meta` object
-------------
At the moment, the `meta` object is only used for nodes that define export
tasks.  It has two relevant functions: `meta.export_stl` and
`meta.export_heightmap`.  When called, each will add a arrow icon to the
top right of the node's representation in the graph; clicking on this arrow
will run the export task.  Both export functions have extensive documentation;
check out the existing export nodes for an example of how they are used.

