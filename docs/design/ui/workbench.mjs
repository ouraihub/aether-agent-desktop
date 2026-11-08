import { resolveTheme, webProperties } from './theme.mjs';

const paths = {
    folder: 'M3 5h6l2 2h10v12H3z',
    terminal: 'M3 4h18v16H3z M6 8l4 4-4 4 M13 16h5',
    branch: 'M6 3v12a3 3 0 0 0 3 3h6 M6 8h8a3 3 0 0 0 3-3V3 M4 3h4 M15 3h4 M15 16v4',
    chevron: 'M9 5l7 7-7 7',
    plus: 'M12 5v14 M5 12h14',
    close: 'M6 6l12 12 M18 6L6 18',
    chat: 'M3 4h18v13H9l-6 4z',
    file: 'M5 3h9l5 5v13H5z M14 3v6h5 M8 13h8 M8 17h6',
    search: 'M10 3a7 7 0 1 0 0 14 7 7 0 0 0 0-14 M15 15l6 6',
    settings: 'M9 3h6l1 4 4 2v6l-4 2-1 4H9l-1-4-4-2V9l4-2z M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8',
    spark: 'M12 2l3 7 7 3-7 3-3 7-3-7-7-3 7-3z',
    arrow: 'M12 20V4 M5 11l7-7 7 7',
    help: 'M9 8a3 3 0 0 1 6 0c0 3-3 2-3 5 M12 17v1 M12 2a10 10 0 1 0 0 20 10 10 0 0 0 0-20'
};
for (const element of document.querySelectorAll('[data-icon]')) {
    const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    svg.setAttribute('viewBox', '0 0 24 24');
    svg.setAttribute('fill', 'none');
    svg.setAttribute('stroke', 'currentColor');
    svg.setAttribute('stroke-width', '1.5');
    svg.setAttribute('stroke-linecap', 'round');
    svg.setAttribute('stroke-linejoin', 'round');
    svg.setAttribute('aria-hidden', 'true');
    const path = document.createElementNS(svg.namespaceURI, 'path');
    path.setAttribute('d', paths[element.dataset.icon]);
    svg.append(path);
    element.append(svg);
}

try {
    const response = await fetch('theme-tokens.json');
    if (!response.ok) throw new Error('主题读取失败');
    const source = await response.json();
    function applyTheme() {
        const appearance = document.documentElement.dataset.theme;
        const accent = document.querySelector('#accent').value;
        const properties = webProperties(source, resolveTheme(source, appearance, accent));
        for (const [name, value] of Object.entries(properties)) {
            document.documentElement.style.setProperty(name, value);
        }
        document.documentElement.style.colorScheme = appearance;
        document.querySelector('#appearance').textContent = appearance === 'dark' ? '浅色' : '深色';
    }
    applyTheme();
    document.querySelector('#appearance').addEventListener('click', () => {
        document.documentElement.dataset.theme = document.documentElement.dataset.theme === 'dark' ? 'light' : 'dark';
        applyTheme();
    });
    document.querySelector('#accent').addEventListener('change', applyTheme);
    const setProjects = show => {
        document.querySelector('#projects').hidden = !show;
        document.querySelector('#projects-toggle').classList.toggle('active', show);
    };
    document.querySelector('#projects-toggle').addEventListener('click', () => setProjects(document.querySelector('#projects').hidden));
    document.querySelector('#projects-close').addEventListener('click', () => setProjects(false));
    const setTerminal = show => {
        document.querySelector('#terminal').hidden = !show;
        document.querySelector('#terminal-toggle').classList.toggle('active', show);
    };
    document.querySelector('#terminal-toggle').addEventListener('click', () => setTerminal(document.querySelector('#terminal').hidden));
    document.querySelector('#terminal-close').addEventListener('click', () => setTerminal(false));
    document.querySelector('#todo-toggle').addEventListener('click', () => {
        const list = document.querySelector('#todo-list');
        list.hidden = !list.hidden;
    });
    document.documentElement.dataset.ready = 'true';
} catch (failure) {
    const error = document.querySelector('#error');
    error.textContent = String(failure) + '；请通过本地 HTTP 服务器打开预览，不是 GTK 程序。';
    error.hidden = false;
}
