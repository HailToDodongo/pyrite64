/**
 * GridHelper.js
 * N64-unit grid overlay for the 3D viewport.
 *
 * Renders a 1-unit ground grid (matching N64 coordinate scale)
 * with a brighter 8-unit major grid and an origin axis indicator.
 *
 * Color scheme matches the Pyrite64 dark editor theme.
 */
import * as THREE from 'three';
export class GridHelper {
    constructor(scene) {
        this.group = new THREE.Group();
        this.group.name = '__editor_grid__';
        // Minor grid: 1-unit divisions, 32×32 total
        this.minorGrid = new THREE.GridHelper(32, 32, 0x333355, 0x222244);
        this.minorGrid.material.opacity = 0.4;
        this.minorGrid.material.transparent = true;
        // Major grid: 8-unit divisions
        this.majorGrid = new THREE.GridHelper(32, 4, 0x4455aa, 0x4455aa);
        this.majorGrid.material.opacity = 0.6;
        this.majorGrid.material.transparent = true;
        this.majorGrid.position.y = 0.001; // prevent z-fighting
        // Axes: X=red, Y=green, Z=blue — matches N64 / Blender Y-up
        this.axes = new THREE.AxesHelper(2);
        this.axes.position.y = 0.002;
        this.group.add(this.minorGrid, this.majorGrid, this.axes);
        scene.add(this.group);
    }
    setVisible(visible) {
        this.group.visible = visible;
    }
    setY(y) {
        this.group.position.y = y;
    }
    dispose() {
        this.minorGrid.geometry.dispose();
        this.majorGrid.geometry.dispose();
        this.axes.geometry.dispose();
        // Dispose materials to prevent memory leaks
        this.minorGrid.material.dispose();
        this.majorGrid.material.dispose();
        this.axes.material.dispose();
    }
}
