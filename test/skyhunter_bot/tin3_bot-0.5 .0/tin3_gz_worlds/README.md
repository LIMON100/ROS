# tin3_gz_worlds

Gazebo world files and models for TIN3 simulation.

---

## Package Contents

```
tin3_gz_worlds/
├── worlds/               # SDF world files
│   ├── empty_world.sdf
│   └── route66.sdf
└── models/               # Gazebo models
    └── <model_name>/
        ├── meshes/       # 3D mesh files
        ├── model.config  # Model metadata
        └── model.sdf     # Model description
```

---

## Adding a New World

1. Place `.sdf` file in `worlds/`
2. Launch with: `ros2 launch tin3_gz_simulation sim.launch.py world:=your_world.sdf`

## Adding a New Model

1. Create a folder under `models/` with the model name
2. Add the standard Gazebo model structure:
   - `model.config` — model metadata (name, author, description)
   - `model.sdf` — SDF model definition
   - `meshes/` — any referenced mesh files (OBJ, DAE, STL)
3. Reference the model by name in your world SDF — the package resource path resolves it automatically

---

## Dependencies

None — this is a resource-only package, used by `tin3_gz_simulation`.