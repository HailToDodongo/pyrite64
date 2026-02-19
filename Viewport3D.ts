/**
 * Viewport3D.ts
 * Pyrite64 — Three.js 3D scene preview viewport
 *
 * Renders the editor scene graph in realtime using Three.js,
 * applying N64-accurate constraints and optional cartoon preview mode.
 *
 * Designed to slot into the existing Electron renderer layout.
 * The viewport reads from a shared SceneGraph data model and is
 * kept in sync via events, NOT by owning the data itself.
 */

import * as THREE from 'three';
import { EffectComposer } from 'three/examples/jsm/postprocessing/EffectComposer';
import { RenderPass } from 'three/examples/jsm/postprocessing/RenderPass';
import { OutlinePass } from 'three/examples/jsm/postprocessing/OutlinePass';
import { N64MaterialBridge } from './N64MaterialBridge';
import { CameraController } from './CameraController';
import { CartoonPass } from './CartoonPass';
import { GridHelper } from './GridHelper';

// ─── N64 Hardware Constraints ─────────────────────────────────────────────────

export const N64_LIMITS = {
  MAX_TRIS_PER_MESH:   64,      // soft budget — RDP display list pressure
  MAX_VERTS_PER_FRAME: 800,     // total scene budget
  VALID_TEX_SIZES:     [32, 64, 128, 256] as const,
  BIG_TEX_SIZE:        256,     // requires big-tex render mode
  RDRAM_TOTAL_BYTES:   4 * 1024 * 1024,   // 4MB base
  FIXED_POINT_SCALE:   65536,   // 16.16 fixed point
} as const;

// ─── Types ────────────────────────────────────────────────────────────────────

export type RenderMode = 'standard' | 'cartoon' | 'wireframe' | 'n64-accurate';

export interface ViewportOptions {
  container: HTMLElement;
  onBudgetWarning?: (warning: BudgetWarning) => void;
  onSelectionChange?: (nodeIds: string[]) => void;
}

export interface BudgetWarning {
  type:    'tris' | 'verts' | 'texture' | 'rdram';
  message: string;
  nodeId?: string;
}

export interface SceneNodeData {
  id:       string;
  name:     string;
  type:     'mesh' | 'light' | 'camera' | 'empty' | 'collision';
  position: [number, number, number];
  rotation: [number, number, number];  // Euler XYZ degrees
  scale:    [number, number, number];
  visible:  boolean;
  // Mesh-specific
  gltfPath?:      string;
  materialId?:    string;
  cartoonBands?:  number;  // cel-shading colour band count (2–8)
}

// ─── Viewport3D ───────────────────────────────────────────────────────────────

export class Viewport3D {
  private renderer:    THREE.WebGLRenderer;
  private scene:       THREE.Scene;
  private camera:      THREE.PerspectiveCamera;
  private composer:    EffectComposer;
  private outlinePass: OutlinePass;
  private cartoonPass: CartoonPass;
  private grid:        GridHelper;
  private camCtrl:     CameraController;
  private matBridge:   N64MaterialBridge;

  private nodeMap   = new Map<string, THREE.Object3D>();
  private selected  = new Set<string>();
  private renderMode: RenderMode = 'standard';
  private animFrameId: number | null = null;

  private budgetWarningCb?: (w: BudgetWarning) => void;
  private selectionChangeCb?: (ids: string[]) => void;

  // ── Construction ────────────────────────────────────────────────────────────

  constructor(opts: ViewportOptions) {
    this.budgetWarningCb  = opts.onBudgetWarning;
    this.selectionChangeCb = opts.onSelectionChange;

    // Renderer
    this.renderer = new THREE.WebGLRenderer({ antialias: true, alpha: false });
    this.renderer.setPixelRatio(window.devicePixelRatio);
    this.renderer.shadowMap.enabled = true;
    this.renderer.outputColorSpace = THREE.SRGBColorSpace;
    opts.container.appendChild(this.renderer.domElement);

    // Camera — matches tiny3d default FOV and near/far
    this.camera = new THREE.PerspectiveCamera(60, 1, 0.01, 500);
    this.camera.position.set(5, 5, 10);
    this.camera.lookAt(0, 0, 0);

    // Scene
    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0x1a1a2e);

    // Ambient + directional light (approximates N64 ambient + sun setup)
    const ambient = new THREE.AmbientLight(0xffffff, 0.4);
    const sun     = new THREE.DirectionalLight(0xfff0d0, 1.2);
    sun.position.set(8, 16, 8);
    sun.castShadow = true;
    this.scene.add(ambient, sun);

    // Grid
    this.grid = new GridHelper(this.scene);

    // Camera controller (orbit + fly)
    this.camCtrl = new CameraController(this.camera, this.renderer.domElement);

    // Post-processing pipeline
    this.composer    = new EffectComposer(this.renderer);
    const renderPass = new RenderPass(this.scene, this.camera);
    this.composer.addPass(renderPass);

    this.outlinePass = new OutlinePass(
      new THREE.Vector2(opts.container.clientWidth, opts.container.clientHeight),
      this.scene,
      this.camera,
    );
    this.outlinePass.edgeStrength  = 3.0;
    this.outlinePass.edgeThickness = 1.5;
    this.outlinePass.visibleEdgeColor.set(0xffcc00);  // N64 gold selection
    this.composer.addPass(this.outlinePass);

    this.cartoonPass = new CartoonPass();
    this.cartoonPass.enabled = false;
    this.composer.addPass(this.cartoonPass);

    // Material bridge (Fast64 → Three.js)
    this.matBridge = new N64MaterialBridge();

    // Resize observer
    const ro = new ResizeObserver(() => this.handleResize(opts.container));
    ro.observe(opts.container);
    this.handleResize(opts.container);

    // Selection via raycasting
    this.renderer.domElement.addEventListener('click', (e) => this.handleClick(e));

    this.startLoop();
  }

  // ── Public API ──────────────────────────────────────────────────────────────

  /** Sync a node from the scene graph data model into the Three.js scene. */
  upsertNode(data: SceneNodeData): void {
    let obj = this.nodeMap.get(data.id);

    if (!obj) {
      obj = this.createObject(data);
      this.nodeMap.set(data.id, obj);
      this.scene.add(obj);
    }

    this.applyTransform(obj, data);
    obj.visible = data.visible;
    obj.name    = data.name;

    this.checkBudgets();
  }

  /** Remove a node from the Three.js scene. */
  removeNode(id: string): void {
    const obj = this.nodeMap.get(id);
    if (obj) {
      this.scene.remove(obj);
      this.nodeMap.delete(id);
      this.selected.delete(id);
    }
  }

  /** Highlight selected nodes with the outline pass. */
  setSelection(ids: string[]): void {
    this.selected = new Set(ids);
    const objects = ids
      .map(id => this.nodeMap.get(id))
      .filter((o): o is THREE.Object3D => !!o);
    this.outlinePass.selectedObjects = objects;
  }

  /** Switch between standard / cartoon / wireframe / n64-accurate render modes. */
  setRenderMode(mode: RenderMode): void {
    this.renderMode = mode;
    this.cartoonPass.enabled = (mode === 'cartoon');

    this.nodeMap.forEach((obj) => {
      obj.traverse((child) => {
        if (child instanceof THREE.Mesh) {
          switch (mode) {
            case 'wireframe':
              (child.material as THREE.Material & { wireframe?: boolean }).wireframe = true;
              break;
            case 'cartoon':
              child.material = this.matBridge.toCartoon(child.material);
              break;
            case 'n64-accurate':
              child.material = this.matBridge.toN64Accurate(child.material);
              break;
            default:
              child.material = this.matBridge.toStandard(child.material);
              (child.material as THREE.Material & { wireframe?: boolean }).wireframe = false;
          }
        }
      });
    });
  }

  /** Toggle the N64 unit grid. */
  setGridVisible(visible: boolean): void {
    this.grid.setVisible(visible);
  }

  /** Frame all objects in view (like pressing F in Blender). */
  frameAll(): void {
    const box = new THREE.Box3();
    this.scene.traverse((obj) => {
      if (obj instanceof THREE.Mesh) box.expandByObject(obj);
    });
    if (!box.isEmpty()) this.camCtrl.fitToBox(box);
  }

  /** Frame a specific node by id. */
  frameNode(id: string): void {
    const obj = this.nodeMap.get(id);
    if (!obj) return;
    const box = new THREE.Box3().setFromObject(obj);
    this.camCtrl.fitToBox(box);
  }

  dispose(): void {
    if (this.animFrameId !== null) cancelAnimationFrame(this.animFrameId);
    this.renderer.dispose();
    this.camCtrl.dispose();
  }

  // ── Private ─────────────────────────────────────────────────────────────────

  private createObject(data: SceneNodeData): THREE.Object3D {
    switch (data.type) {
      case 'mesh': {
        // Placeholder geometry until GLTF is loaded
        const geo  = new THREE.BoxGeometry(1, 1, 1);
        const mat  = this.matBridge.createDefault();
        const mesh = new THREE.Mesh(geo, mat);
        mesh.castShadow    = true;
        mesh.receiveShadow = true;
        // TODO: if data.gltfPath, trigger async GLTF load and swap geometry
        return mesh;
      }
      case 'light': {
        const light = new THREE.PointLight(0xffffff, 1, 20);
        // Add a small helper sphere so lights are visible in editor
        const helper = new THREE.Mesh(
          new THREE.SphereGeometry(0.1, 8, 8),
          new THREE.MeshBasicMaterial({ color: 0xffff88 }),
        );
        light.add(helper);
        return light;
      }
      case 'camera': {
        const camHelper = new THREE.CameraHelper(
          new THREE.PerspectiveCamera(60, 1.77, 0.1, 100),
        );
        return camHelper;
      }
      case 'collision': {
        // Render collision volumes as transparent blue wireframe
        const geo = new THREE.BoxGeometry(1, 1, 1);
        const mat = new THREE.MeshBasicMaterial({
          color: 0x4488ff, wireframe: true, opacity: 0.4, transparent: true,
        });
        return new THREE.Mesh(geo, mat);
      }
      default:
        return new THREE.Object3D();  // empty / group
    }
  }

  private applyTransform(obj: THREE.Object3D, data: SceneNodeData): void {
    obj.position.set(...data.position);
    obj.rotation.set(
      THREE.MathUtils.degToRad(data.rotation[0]),
      THREE.MathUtils.degToRad(data.rotation[1]),
      THREE.MathUtils.degToRad(data.rotation[2]),
    );
    obj.scale.set(...data.scale);
  }

  private handleClick(event: MouseEvent): void {
    const rect   = this.renderer.domElement.getBoundingClientRect();
    const mouse  = new THREE.Vector2(
      ((event.clientX - rect.left) / rect.width)  * 2 - 1,
      -((event.clientY - rect.top) / rect.height) * 2 + 1,
    );
    const ray    = new THREE.Raycaster();
    ray.setFromCamera(mouse, this.camera);

    const hits = ray.intersectObjects([...this.nodeMap.values()], true);
    if (hits.length === 0) {
      this.setSelection([]);
      this.selectionChangeCb?.([]);
      return;
    }

    // Walk up to find the root node in our nodeMap
    let target: THREE.Object3D | null = hits[0].object;
    while (target && target.parent !== this.scene) target = target.parent;
    if (!target) return;

    const id = [...this.nodeMap.entries()]
      .find(([, obj]) => obj === target)?.[0];
    if (!id) return;

    const newSelection = event.shiftKey
      ? [...this.selected, id]
      : [id];

    this.setSelection(newSelection);
    this.selectionChangeCb?.(newSelection);
  }

  private checkBudgets(): void {
    let totalVerts = 0;
    this.nodeMap.forEach((obj, id) => {
      obj.traverse((child) => {
        if (child instanceof THREE.Mesh) {
          const triCount = (child.geometry.index
            ? child.geometry.index.count / 3
            : child.geometry.attributes.position.count / 3);

          if (triCount > N64_LIMITS.MAX_TRIS_PER_MESH) {
            this.budgetWarningCb?.({
              type:    'tris',
              nodeId:  id,
              message: `"${child.name}" has ${Math.round(triCount)} tris — exceeds N64 budget of ${N64_LIMITS.MAX_TRIS_PER_MESH}`,
            });
          }
          totalVerts += child.geometry.attributes.position.count;
        }
      });
    });

    if (totalVerts > N64_LIMITS.MAX_VERTS_PER_FRAME) {
      this.budgetWarningCb?.({
        type:    'verts',
        message: `Scene total ${totalVerts} verts exceeds safe N64 frame budget (${N64_LIMITS.MAX_VERTS_PER_FRAME})`,
      });
    }
  }

  private handleResize(container: HTMLElement): void {
    const w = container.clientWidth;
    const h = container.clientHeight;
    this.camera.aspect = w / h;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(w, h);
    this.composer.setSize(w, h);
  }

  private startLoop(): void {
    const tick = () => {
      this.animFrameId = requestAnimationFrame(tick);
      this.camCtrl.update();
      this.composer.render();
    };
    tick();
  }
}
