# PSP Physics Scene-Binding Trace Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Emit durable PSP boot-log records that identify every BEPU body binding during scene activation.

**Architecture:** `BepuPhysicsWorld3D` owns the registration sequence, so it will emit optional begin, before-body, after-body, and end records through a maintained callback. The PSP host will install a callback that writes those records with `PspBootTrace`; generated C++ is only regenerated from the maintained C# source and is never edited.

**Tech Stack:** .NET 9, xUnit, C# source-to-C++ generation, C++20, PSPSDK.

## Global Constraints

- Keep the probe observational: do not alter BEPU attachment, solver configuration, serialized scenes, or exception behavior.
- Include deterministic binding index, rigid-body kind, collider kind, and registered-body count in each per-body record.
- Do not edit generated C++ output.
- Follow repository C# and C++ documentation, member-order, naming, and formatting rules.

---

## File Structure

- Modify: `C:\dev\helworks\helengine\engine\helengine.bepu\BepuPhysicsWorld3D.cs` — owns and emits binding records at the exact registration boundary.
- Modify: `C:\dev\helworks\helengine\engine\helengine.bepu.tests\BepuPhysicsWorld3DTests.cs` — establishes ordered binding-record behavior from maintained C# source.
- Modify: `src/platform/psp/PspBootHost.cpp` — installs the PSP-specific trace sink after creating the physics world.
- Modify: `builder.tests/PspPackagedRuntimeSourceTests.cs` — locks the PSP host wiring contract.

### Task 1: Specify and test ordered binding records

**Files:**
- Modify: `C:\dev\helworks\helengine\engine\helengine.bepu.tests\BepuPhysicsWorld3DTests.cs`

**Produces:** a failing test requiring a static ground body and one dynamic box to emit deterministic bind records.

- [x] **Step 1: Write the failing test**

```csharp
[Fact]
public void BindScene_WithDiagnosticSink_ReportsEachRegisteredBodyInOrder() {
    List<string> records = new List<string>();
    BepuPhysicsWorld3D world = BepuPhysicsWorld3D.CreateDefault();
    world.SceneBindingDiagnosticSink = records.Add;

    world.BindScene([CreateStaticBoxEntity(), CreateDynamicBoxEntity()]);

    Assert.Equal([
        "PhysicsBind begin roots=2",
        "PhysicsBind before index=1 body=Static collider=Box",
        "PhysicsBind after index=1 body=Static collider=Box bodies=1",
        "PhysicsBind before index=2 body=Dynamic collider=Box",
        "PhysicsBind after index=2 body=Dynamic collider=Box bodies=2",
        "PhysicsBind end bodies=2"
    ], records);
}
```

- [x] **Step 2: Run the focused test to verify red**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.bepu.tests\helengine.bepu.tests.csproj --filter BindScene_WithDiagnosticSink_ReportsEachRegisteredBodyInOrder
```

Expected: compile failure because `SceneBindingDiagnosticSink` does not exist.

### Task 2: Emit source-owned binding diagnostics

**Files:**
- Modify: `C:\dev\helworks\helengine\engine\helengine.bepu\BepuPhysicsWorld3D.cs`

**Consumes:** `Action<string>` sink configured by the host.

**Produces:** `SceneBindingDiagnosticSink` and records emitted immediately before and after every supported body registration.

- [x] **Step 1: Add the optional diagnostic property after existing public world properties**

```csharp
/// <summary>
/// Gets or sets the optional sink that receives scene-binding records synchronously at each body-registration boundary.
/// </summary>
public Action<string> SceneBindingDiagnosticSink { get; set; }
```

- [x] **Step 2: Add three private emission methods**

```csharp
void WriteSceneBindingDiagnostic(string message) {
    Action<string> diagnosticSink = SceneBindingDiagnosticSink;
    if (diagnosticSink != null) {
        diagnosticSink(message);
    }
}

void WriteBodyBindingDiagnostic(string phase, int bindingIndex, RigidBody3DComponent rigidBody, string colliderKind) {
    WriteSceneBindingDiagnostic(
        "PhysicsBind " + phase
        + " index=" + bindingIndex
        + " body=" + rigidBody.BodyKind
        + " collider=" + colliderKind
        + (phase == "after" ? " bodies=" + RegisteredBodyCount : string.Empty));
}

string ResolveColliderKind(BoxCollider3DComponent boxCollider, SphereCollider3DComponent sphereCollider, StaticMeshCollider3DComponent staticMeshCollider) {
    if (boxCollider != null) {
        return "Box";
    } else if (sphereCollider != null) {
        return "Sphere";
    }

    return "StaticMesh";
}
```

- [x] **Step 3: Emit records in `BindScene` and `RegisterEntityIfSupported`**

```csharp
WriteSceneBindingDiagnostic("PhysicsBind begin roots=" + rootEntities.Count);
// Existing reset and hierarchy-registration logic remains unchanged.
WriteSceneBindingDiagnostic("PhysicsBind end bodies=" + RegisteredBodyCount);
```

```csharp
string colliderKind = ResolveColliderKind(boxCollider, sphereCollider, staticMeshCollider);
int bindingIndex = RegisteredBodyCount + 1;
WriteBodyBindingDiagnostic("before", bindingIndex, rigidBody, colliderKind);
if (staticMeshCollider != null) {
    RegisterStaticMeshBody(entity, rigidBody, staticMeshCollider);
    WriteBodyBindingDiagnostic("after", bindingIndex, rigidBody, colliderKind);
    return;
} else if (boxCollider != null) {
    RegisterBoxBody(entity, rigidBody, boxCollider);
    WriteBodyBindingDiagnostic("after", bindingIndex, rigidBody, colliderKind);
    return;
}

RegisterSphereBody(entity, rigidBody, sphereCollider);
WriteBodyBindingDiagnostic("after", bindingIndex, rigidBody, colliderKind);
```

- [x] **Step 4: Run the focused test to verify green**

Run the Task 1 command.

Expected: PASS.

### Task 3: Forward generated diagnostics into the PSP boot trace

**Files:**
- Modify: `src/platform/psp/PspBootHost.cpp`
- Modify: `builder.tests/PspPackagedRuntimeSourceTests.cs`

**Consumes:** generated `BepuPhysicsWorld3D::SceneBindingDiagnosticSink` support.

**Produces:** PSP-only forwarding from each synchronous binding record to `helengine_psp_boot.log`.

- [x] **Step 1: Write the failing PSP source-contract test**

```csharp
[Fact]
public void PspBootHost_forwards_bepu_scene_binding_diagnostics_to_boot_trace() {
    string source = File.ReadAllText(PspBootHostPath);

    Assert.Contains("physicsWorld->set_SceneBindingDiagnosticSink(", source, StringComparison.Ordinal);
    Assert.Contains("new Action<std::string>([](std::string message) {", source, StringComparison.Ordinal);
    Assert.Contains("PspBootTrace::WriteLine(std::string(\"PhysicsBinding \") + message);", source, StringComparison.Ordinal);
}
```

- [x] **Step 2: Run the focused source test to verify red**

Run:

```powershell
rtk dotnet test builder.tests\helengine.psp.builder.tests.csproj --filter PspBootHost_forwards_bepu_scene_binding_diagnostics_to_boot_trace
```

Expected: FAIL because the host does not configure the sink.

- [x] **Step 3: Wire the sink directly after world construction**

```cpp
BepuPhysicsWorld3D* physicsWorld = BepuPhysicsWorld3D::CreateWithSolveSchedule(2, 1);
physicsWorld->set_SceneBindingDiagnosticSink(
    new Action<std::string>([](std::string message) {
        PspBootTrace::WriteLine(std::string("PhysicsBinding ") + message);
    }));
BepuRuntimeComponentRegistration::AttachRuntimeWorld(EngineCore, physicsWorld);
```

The generated setter accepts `Action<std::string>*`, so use `new Action<std::string>([](std::string message) { ... })` around the lambda while retaining exact synchronous forwarding behavior.

- [x] **Step 4: Run the focused PSP source test to verify green**

Run the Task 3 Step 2 command.

Expected: PASS.

### Task 4: Regenerate, build, and obtain the hardware boundary

**Files:**
- No additional source files.

- [x] **Step 1: Run the focused BEPU and PSP source tests**

```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.bepu.tests\helengine.bepu.tests.csproj --filter BindScene_WithDiagnosticSink_ReportsEachRegisteredBodyInOrder
rtk dotnet test builder.tests\helengine.psp.builder.tests.csproj --filter PspBootHost_forwards_bepu_scene_binding_diagnostics_to_boot_trace
```

Expected: both pass.

- [ ] **Step 2: Regenerate the PSP core and build the demo-disc artifact**

Use the normal PSP build command for `C:\dev\helprojs\demodisc\project.heproj`, then deploy the resulting `HELENGINE` folder to Adrenaline.

Current blocker: the native PSP compile reaches the generated unity translation unit but fails in unrelated generated shader code: `ShaderCompileRequestIdentity.cpp` references undeclared `Convert` in `CreateDeviceJobHash`. The generated source must be corrected through its maintained C# generation input before this PSP package can complete.

- [ ] **Step 3: Reproduce and inspect the final binding record**

Open Stacked Boxes and copy `helengine_psp_boot.log`. The final `PhysicsBinding PhysicsBind before ...` without a matching `after` record identifies the exact failing operation; no root-cause fix is included in this plan.

- [ ] **Step 4: Commit the implementation**

```powershell
git -C C:\dev\helworks\helengine add -- engine\helengine.bepu\BepuPhysicsWorld3D.cs engine\helengine.bepu.tests\BepuPhysicsWorld3DTests.cs
git -C C:\dev\helworks\helengine commit -m "feat: trace BEPU scene body binding"
git -C C:\dev\helworks\helengine-psp add -- src\platform\psp\PspBootHost.cpp builder.tests\PspPackagedRuntimeSourceTests.cs
git -C C:\dev\helworks\helengine-psp commit -m "feat: trace PSP physics scene binding"
```
