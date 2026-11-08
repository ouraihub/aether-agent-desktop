// Design tooling only: one token source, separate Web/GTK emitters.
const color = /^#[0-9a-f]{6}$/i;
const name = /^[a-z][a-z0-9_.]*$/;
const key = value => value.replaceAll('.', '_');

export function resolveTheme(source, appearance, accent) {
    if (source.schema_version !== 1) throw new Error('Unsupported token schema');
    const palette = source.palettes[appearance];
    const selected = source.accents[accent]?.[appearance];
    if (!palette || !selected) throw new Error('Unknown appearance or accent');
    const resolved = {};
    for (const [token, reference] of Object.entries(source.semantic)) {
        if (!name.test(token)) throw new Error('Invalid token name');
        const [group, field] = reference.split('.');
        const value = group === 'palette' ? palette[field] : group === 'accent' ? selected[field] : null;
        if (!color.test(value)) throw new Error('Invalid semantic color: ' + token);
        resolved[token] = value;
    }
    for (const [token, reference] of Object.entries(source.components)) {
        if (!name.test(token) || !resolved[reference]) throw new Error('Invalid component token');
        resolved[token] = resolved[reference];
    }
    return resolved;
}

export function webProperties(source, resolved) {
    const result = {};
    for (const [token, value] of Object.entries(resolved)) {
        result['--aether-' + token.replaceAll('.', '-')] = value;
    }
    for (const [alias, token] of Object.entries(source.web_aliases)) {
        if (!name.test(alias) || !resolved[token]) throw new Error('Invalid web alias');
        result['--' + alias] = resolved[token];
    }
    const metrics = source.metrics;
    for (const metric of ['font_body', 'font_code', 'radius_control', 'radius_panel', 'focus_width']) {
        if (!Number.isFinite(metrics[metric]) || metrics[metric] <= 0) throw new Error('Invalid metric');
        result['--aether-' + metric.replaceAll('_', '-')] = metrics[metric] + 'px';
    }
    return result;
}

export function gtkCss(source, resolved) {
    // GTK 4.14-compatible named colors. No Web var() dependency in GTK output.
    webProperties(source, resolved); // Validate shared metrics and alias references.
    const colors = Object.entries(resolved).map(([token, value]) =>
        '@define-color aether_' + key(token) + ' ' + value + ';');
    const metrics = source.metrics;
    return colors.join('\n') + '\n' + [
        '.aether-root { background-color: @aether_bg_canvas; color: @aether_text_primary; }',
        '.aether-panel { background-color: @aether_bg_panel; }',
        '.aether-nav { background-color: @aether_navigation_bg; color: @aether_navigation_text; }',
        '.aether-nav:selected, .aether-nav.selected { background-color: @aether_navigation_selected; }',
        '.aether-message { background-color: @aether_message_bg; color: @aether_message_text; }',
        '.aether-editor { background-color: @aether_editor_bg; border-color: @aether_editor_border; }',
        '.aether-primary { background-image: none; background-color: @aether_button_bg; color: @aether_button_text; }',
        '.aether-secondary:hover { background-color: @aether_control_hover; }',
        '.aether-root :focus-visible { outline-color: @aether_focus_ring; outline-style: solid;',
        '  outline-width: ' + metrics.focus_width + 'px; outline-offset: 2px; }',
        '.aether-root button { border-radius: ' + metrics.radius_control + 'px; }',
        '.aether-error { color: @aether_status_error; }',
        '.aether-warning { color: @aether_status_warning; }',
        '.aether-success { color: @aether_status_success; }'
    ].join('\n') + '\n';
}
