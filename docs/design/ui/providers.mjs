import { resolveTheme, webProperties } from './theme.mjs';

const $ = selector => document.querySelector(selector);
const esc = value => String(value).replace(/[&<>"']/g, char => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[char]));
const paths = {
    close: 'm6 6 12 12M6 18 18 6', search: 'm21 21-5-5M18 10a8 8 0 1 1-16 0 8 8 0 0 1 16 0',
    plus: 'M12 5v14M5 12h14', minus: 'M5 12h14', more: 'M5 12h.01M12 12h.01M19 12h.01',
    down: 'm6 9 6 6 6-6', right: 'm9 6 6 6-6 6', fold: 'm7 4 5 5 5-5M7 20l5-5 5 5',
    settings: 'M4 7h16M4 17h16M8 4v6M16 14v6', filter: 'M4 7h16M4 17h16M8 4v6M16 14v6',
    key: 'M14 7a5 5 0 1 1-2 7L5 21H2v-3l7-7a5 5 0 0 1 5-4ZM16 7h.01',
    eye: 'M2 12s4-7 10-7 10 7 10 7-4 7-10 7S2 12 2 12ZM15 12a3 3 0 1 1-6 0 3 3 0 0 1 6 0',
    copy: 'M9 9h11v12H9zM5 15H3V3h12v2', edit: 'm15 5 4 4M4 20l4-1L21 6l-4-4L4 15z',
    trash: 'M3 6h18M9 6V3h6v3M6 6l1 15h10l1-15M10 10v7M14 10v7',
    sync: 'M20 7a9 9 0 0 0-15-2L2 8M2 3v5h5M4 17a9 9 0 0 0 15 2l3-3M22 21v-5h-5',
    brain: 'M12 3v18M12 5C6 0 2 7 5 10c-5 4 0 10 4 8 0 4 3 3 3 1M12 5c6-5 10 2 7 5 5 4 0 10-4 8 0 4-3 3-3 1',
    tools: 'm14 6 4 4 4-4c2 7-4 10-8 7l-8 8-3-3 8-8C8 5 11 1 16 2z',
    terminal: 'M3 4h18v16H3zM6 8l4 4-4 4M13 16h5', plug: 'M8 3v5M16 3v5M5 8h14v4a7 7 0 0 1-14 0zM12 19v3',
    logs: 'M6 3h14v18H6V3ZM9 7h8M9 11h8M9 15h5', palette: 'M12 3a9 9 0 1 0 0 18c4 0-2-6 2-6h3c6 0 4-12-5-12ZM7 9h.01M11 6h.01M16 8h.01M6 14h.01'
};
const icon = name => `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="${paths[name] || paths.settings}"/></svg>`;
const icons = (root = document) => root.querySelectorAll('[data-icon]').forEach(node => { node.innerHTML = icon(node.dataset.icon); });
const button = (id, name, title, classes = 'icon-button ghost') => `<button type="button" id="${id}" class="${classes}" title="${title}" aria-label="${title}">${icon(name)}</button>`;
const field = (title, content) => `<div class="field"><span class="section-label">${title}</span>${content}</div>`;
const input = (id, value = '', placeholder = '') => `<input id="${id}" value="${esc(value)}" placeholder="${esc(placeholder)}" autocomplete="off">`;
const short = { chat: 'OpenAI', responses: 'Responses', anthropic: 'Anthropic' };
const protocols = { chat: 'OpenAI 兼容（Chat Completions）', responses: 'OpenAI Responses', anthropic: 'Anthropic 兼容（Messages）' };
const types = ['对话', '图像', '视频', '语音合成', '语音识别', '向量', '重排', '未知'];
const options = (items, selected) => Object.entries(items).map(([value, text]) => `<option value="${value}" ${value === selected ? 'selected' : ''}>${text}</option>`).join('');
const protocolSelect = (id, selected, allowed = Object.keys(protocols)) => `<select id="${id}">${options(Object.fromEntries(allowed.map(p => [p, protocols[p]])), selected)}</select>`;
const model = (id, caps, lines = ['chat'], type = '对话') => ({ id, caps, lines, type, name: id, context: '131072', output: '8192' });
const providers = [
    { id: 'qiniu', name: '七牛云', enabled: false, preset: true, keys: [], lines: ['chat'], models: [] },
    { id: 'siliconflow', name: '硅基流动', enabled: false, preset: true, keys: [], lines: ['chat'], models: [] },
    { id: 'fenno', name: 'Fenno', enabled: false, preset: true, keys: [], lines: ['chat'], models: [] }
];
let selected = providers[0], line = 'chat', filter = 'all';
let folded = new Set(), visibleGroups = [];
const populatedFixture = () => {
    selected = providers[0];
    Object.assign(selected, { enabled: true, keys: ['主密钥', '备用'], lines: ['chat', 'responses'], models: [model('deepseek-chat', ['tools']), model('deepseek-reasoner', ['reasoning', 'tools']), model('qwen3-235b', ['vision', 'reasoning', 'tools'], ['chat', 'responses'])] });
    line = 'chat'; folded.clear(); renderRail(); renderDetail();
    notice('切换本地交互示例（不是读取真实配置）');
};
const modal = $('#provider-modal');
const notice = (action = '此操作') => {
    const text = `设计预览未接Backend：${action}未执行；没有网络请求或配置保存。`;
    $('#preview-status').textContent = text;
    if (modal.open) $('#modal-status').textContent = text;
};
const presetUrls = { qiniu: 'https://api.qnaigc.com', siliconflow: 'https://api.siliconflow.cn/v1', fenno: 'https://api.fenno.ai/v1' };
const url = (provider = selected, protocol = line) => protocol === 'chat' ? presetUrls[provider.id] : `https://${provider.id}.example.invalid/${protocol === 'anthropic' ? 'anthropic' : 'responses/v1'}`;
const normalizedBase = (raw, protocol) => {
    const base = raw.trim().replace(/\/+$/, '');
    if (base.endsWith('#')) return base.slice(0, -1);
    if (protocol === 'anthropic') return base.replace(/\/v\d+(?:alpha|beta)?$/i, '');
    return /\/v\d+(?:alpha|beta)?$/i.test(base) ? base : `${base}/v1`;
};
const switchControl = (id, checked, title) => `<button type="button" id="${id}" class="switch" role="switch" aria-checked="${checked}" aria-label="${title}" title="${title}（仅预览，不保存）"></button>`;
function renderRail() {
    const query = $('#provider-search').value.trim().toLowerCase();
    const shown = providers.filter(p => p.name.toLowerCase().includes(query) && (filter === 'all' || p.enabled === (filter === 'enabled')));
    $('#provider-rows').innerHTML = shown.map(p => `<div class="provider-row ${p.id === selected.id ? 'selected' : ''} ${p.enabled ? '' : 'disabled'}" data-provider="${p.id}"><button class="provider-select" type="button" aria-pressed="${p.id === selected.id}" data-select="${p.id}"><span class="avatar">${esc(p.name[0].toUpperCase())}</span><span class="provider-name">${esc(p.name)}</span></button>${p.enabled ? `<span class="status-dot ${p.keys.length ? '' : 'warning'}" title="${p.keys.length ? '启用 · 已配置示例掩码' : '启用 · 无密钥示例'}"></span>` : ''}<button class="small-icon" type="button" data-provider-menu="${p.id}" aria-label="${esc(p.name)} 更多操作">${icon('more')}</button></div>`).join('') || '<p class="empty">没有匹配的服务商</p>';
    $('#providers-filter').setAttribute('aria-label', `筛选服务商：${{ all: '全部', enabled: '启用', disabled: '停用' }[filter]}`);
}
function renderDetail() {
    const hasKey = selected.keys.length > 0;
    $('#provider-detail').innerHTML = `
        <div class="row"><h3 id="provider-name">${esc(selected.name)}</h3>${button('provider-settings', 'settings', '服务商设置')}<span class="grow"></span>${switchControl('provider-enabled', selected.enabled, '启用服务商')}</div>
        <section class="stack" aria-label="API 密钥"><div class="row"><span class="section-label">API 密钥</span>${selected.preset ? '<button class="link" id="provider-get-key">获取密钥</button>' : ''}</div>
            <div class="row">${hasKey ? `<div class="row key-value grow"><span class="metadata">${esc(selected.keys[0])}</span><span class="code key-mask">••••••••</span>${selected.keys.length > 1 ? '<span class="metadata">+1</span>' : ''}<span class="grow"></span><div class="row key-actions"><span class="key-extra"><button class="small-icon" disabled title="Aether 安全存储不回显原密钥，禁止复制明文" aria-label="复制密钥（安全限制，已禁用）">${icon('copy')}</button>${button('provider-key-edit', 'edit', '编辑密钥', 'small-icon')}</span><button id="provider-key-reveal" class="small-icon" disabled title="Aether 安全存储不回显原密钥" aria-label="显示密钥（安全限制，已禁用）">${icon('eye')}</button></div></div>` : '<input id="provider-first-key" type="password" autocomplete="new-password" placeholder="输入新的 API 密钥（留空不修改）" aria-label="新的 API 密钥（仅预览，请勿输入真实密钥）">'}
                ${button('provider-keys', 'key', 'API 密钥管理', 'icon-button')}<button id="provider-check">检测</button></div>
            ${hasKey ? '<p class="security-note">Aether 安全存储限制：不回显、不复制原密钥；仅展示无秘密的掩码样式。</p>' : ''}
        </section>
        <section class="stack strategy"><span class="section-label">密钥使用方式</span><span>轮流使用</span><p class="metadata">每次请求从下一个密钥开始，把限流分摊到各个密钥</p></section>
        <section class="stack"><div class="row"><span class="section-label">API 地址</span>${selected.lines.length < 3 ? '<button id="provider-add-line" class="link">添加线路</button>' : ''}</div>
            ${selected.lines.length > 1 ? `<select id="provider-line" class="line-picker" aria-label="线路">${options(Object.fromEntries(selected.lines.map(p => [p, `${short[p]} · ${url(selected, p)}`])), line)}</select>` : ''}
            <div class="row"><input id="provider-url" class="grow" aria-label="API 地址" value="${url()}">${button('provider-line-settings', 'settings', '配置协议、API 地址与自定义请求头', 'icon-button')}</div><p class="probe" id="provider-probe">GET ${normalizedBase(url(), line)}${line === 'anthropic' ? '/v1/models' : '/models'}</p></section>
        <section class="stack models"><div class="row model-toolbar"><span class="model-title">模型</span>${button('models-fold', 'fold', '全部折叠', 'small-icon')}<label class="search"><span>${icon('search')}</span><input id="model-search" placeholder="搜索模型…" aria-label="搜索模型"></label><select id="models-cap" aria-label="能力筛选"><option value="">全部</option><option value="vision">视觉</option><option value="reasoning">推理</option><option value="tools">工具</option></select><span class="grow"></span><button id="models-sync" class="sync">${icon('sync')}同步模型</button>${button('models-add', 'plus', '添加模型', 'icon-button')}</div><div id="model-list" class="model-list"></div></section>`;
    renderModels();
}
function renderModels() {
    const query = $('#model-search').value.trim().toLowerCase(), cap = $('#models-cap').value;
    const models = selected.models.filter(m => m.lines.includes(line) && (!cap || m.caps.includes(cap)) && `${m.id} ${m.name}`.toLowerCase().includes(query));
    const groups = Map.groupBy(models, m => m.id.split(/[-/:]/)[0] || '其他');
    visibleGroups = [...groups.keys()].sort();
    $('#model-list').innerHTML = visibleGroups.map(group => `<section class="model-group" data-group="${esc(group)}"><div class="group-heading row"><button type="button" class="ghost grow" data-fold="${esc(group)}" aria-expanded="${!folded.has(group)}" style="justify-content:flex-start;padding:0">${icon(folded.has(group) ? 'right' : 'down')}${esc(group)} <span class="metadata">${groups.get(group).length}</span></button><button type="button" class="small-icon" data-write="移除整组" aria-label="移除整组">${icon('minus')}</button></div>${folded.has(group) ? '' : groups.get(group).map(m => `<div class="row model-row" data-model="${esc(m.id)}"><span class="avatar">${esc(m.id[0].toUpperCase())}</span><span class="grow model-id">${esc(m.id)}</span>${m.type === '对话' ? '' : `<span class="metadata">${m.type}</span>`}<span class="capabilities">${m.caps.map(c => `<span class="cap ${c}" title="${{ vision: '视觉', reasoning: '推理', tools: '工具' }[c]}">${icon(c === 'vision' ? 'eye' : c === 'reasoning' ? 'brain' : 'tools')}</span>`).join('')}</span><button type="button" class="small-icon" data-model-settings="${esc(m.id)}" title="模型设置" aria-label="${esc(m.id)} 模型设置">${icon('settings')}</button><button class="small-icon" type="button" data-write="移除模型" aria-label="移除模型">${icon('minus')}</button></div>`).join('')}</section>`).join('') || '<p class="empty">没有匹配的模型 / 未选择模型</p>';
    $('#model-list').classList.toggle('empty-model-list', models.length === 0);
    if (models.length === 0) $('#model-list').innerHTML = `<p class="empty">${selected.models.some(m => m.lines.includes(line)) ? '没有匹配的模型' : '未选择模型'}</p>`;
    $('#models-fold').disabled = visibleGroups.length === 0;
    $('#models-fold').title = visibleGroups.every(g => folded.has(g)) ? '全部展开' : '全部折叠';
    $('#models-fold').setAttribute('aria-label', $('#models-fold').title);
}
function showMenu(trigger, entries) {
    const menu = $('#popup-menu');
    menu.innerHTML = '';
    entries.forEach(entry => {
        const item = document.createElement('button');
        item.type = 'button'; item.textContent = `${entry.checked ? '✓ ' : ''}${entry.label}`;
        item.disabled = Boolean(entry.disabled); item.setAttribute('role', 'menuitem');
        item.onclick = () => { closeMenu(); entry.run(); };
        menu.append(item);
    });
    const rect = trigger.getBoundingClientRect();
    menu.style.left = `${Math.min(rect.left, window.innerWidth - 220)}px`; menu.style.top = `${rect.bottom + 4}px`;
    menu.hidden = false; trigger.setAttribute('aria-expanded', 'true');
    menu.querySelector('button:not(:disabled)')?.focus();
}
function closeMenu() {
    $('#popup-menu').hidden = true;
    document.querySelectorAll('[aria-expanded="true"]').forEach(node => { if (node.id === 'providers-filter' || node.dataset.providerMenu) node.setAttribute('aria-expanded', 'false'); });
}
function openModal(title, width, body, submit = '确定', closeOnly = false) {
    closeMenu();
    $('#modal-title').textContent = title; modal.style.width = `${width}px`; $('#modal-body').innerHTML = body;
    $('#modal-status').textContent = '设计预览未接Backend；填写内容不会保存，请勿输入真实密钥。';
    $('#modal-footer').innerHTML = closeOnly ? '<button type="button" data-close>关闭</button>' : `<button type="button" data-close>取消</button><button type="submit" class="primary">${submit}</button>`;
    if (!modal.open) modal.showModal();
}
function nameDialog(provider) {
    openModal(provider ? '服务商设置' : '新建服务商', 440, `${field('名称', input('new-name', provider?.name))}${provider ? '' : field('协议', protocolSelect('new-protocol', 'chat')) + field('API 地址', input('new-url', '', 'https://api.example.invalid/v1'))}${field('接入方式', '<div class="segment" id="provider-access"><button type="button" data-access="direct" class="selected" aria-pressed="true">直连</button><button type="button" data-access="gateway" aria-pressed="false">网关</button></div>')}<p class="metadata">网关：引擎只连本机，真实密钥不进入引擎进程。</p>`);
}
function lineDialog(editing) {
    const allowed = Object.keys(protocols).filter(p => p === (editing ? line : null) || !selected.lines.includes(p));
    const protocol = editing ? line : allowed[0];
    openModal(editing ? '线路设置' : '添加线路', 520, `${field('协议', protocolSelect('line-protocol', protocol, allowed))}${field('API 地址', input('line-url', editing ? url() : '', 'https://api.example.invalid/v1'))}<p id="line-request" class="probe"></p><p class="metadata">末尾添加 # 可禁用自动附加的 API 版本。</p>${field('自定义请求头', '<div id="header-fields" class="header-fields"></div><button type="button" id="line-header-add" class="ghost">添加请求头</button><p class="metadata">请求头对该服务商的所有线路生效。</p>')}${editing ? `<div class="row">${line === selected.lines[0] ? '<span class="metadata" style="color:var(--accent)">默认</span>' : '<button type="button" id="line-default">设为默认线路</button>'}<span class="grow"></span><button type="button" id="line-remove" class="danger" ${selected.lines.length < 2 ? 'disabled title="至少保留一条线路。"' : ''}>删除线路</button></div>` : ''}`);
    requestPreview();
}
function requestPreview() {
    const value = $('#line-url')?.value || '';
    const protocol = $('#line-protocol')?.value;
    const suffix = { chat: '/chat/completions', responses: '/responses', anthropic: '/v1/messages' }[protocol];
    $('#line-request').textContent = value ? `请求地址：POST ${normalizedBase(value, protocol)}${suffix}` : '';
}
function keysDialog(edit = false) {
    openModal('API 密钥管理', 520, `<p class="metadata">管理当前服务商的多个 API 密钥，按顺序轮流使用。</p>${selected.keys.map((label, i) => `<div class="row key-manager-row"><span class="key-label">${esc(label)}</span><span class="code grow">••••••••</span><button type="button" class="small-icon" disabled title="Aether 安全存储禁止复制原密钥" aria-label="复制密钥（已禁用）">${icon('copy')}</button><button type="button" class="small-icon" data-key-edit="${i}" aria-label="编辑密钥">${icon('edit')}</button><button type="button" class="small-icon" data-write="删除密钥" aria-label="删除密钥">${icon('trash')}</button>${switchControl(`key-enabled-${i}`, true, '启用密钥')}</div>`).join('') || '<p class="metadata">暂无 API 密钥</p>'}<div class="row"><input id="keys-label" class="key-label" placeholder="标签" aria-label="标签" value="${edit ? esc(selected.keys[0] || '') : ''}"><input id="keys-value" type="password" autocomplete="new-password" placeholder="输入新的 API 密钥（留空不修改）" aria-label="新的 API 密钥（仅预览）"><button type="button" id="keys-submit" class="primary">${edit ? '保存' : '添加 API 密钥'}</button></div><p id="keys-keep" class="metadata" ${edit ? '' : 'hidden'}>编辑密钥：输入新值后保存；留空则不修改原密钥。</p><p class="metadata">${selected.keys.length} / ${selected.keys.length} 已启用</p><p class="security-note">显示 / 复制原密钥已禁用；仅输入新值的写入字段被保留。此预览不读取、存储或传输密钥。</p>`, '', true);
}
const check = (id, title, on = false) => `<label class="check"><input type="checkbox" id="${id}" ${on ? 'checked' : ''}>${title}</label>`;
function modelDialog(id) {
    const m = selected.models.find(item => item.id === id);
    openModal('编辑模型', 520, `<p class="code metadata">${esc(m.id)}</p><div class="row">${field('显示名称', input('model-name', m.name))}${field('类型', `<select id="model-type">${types.map(t => `<option ${t === m.type ? 'selected' : ''}>${t}</option>`).join('')}</select>`)}</div>${field('线路（首项为默认）', `<div class="row">${selected.lines.map(p => check(`model-line-${p}`, `${short[p]}${m.lines[0] === p ? ' · 默认' : ''}`, m.lines.includes(p))).join('')}</div>`)}<div class="row">${field('上下文窗口', input('model-context', m.context))}${field('最大输出', input('model-output', m.output))}</div>${field('能力', `<div class="row">${['vision', 'reasoning', 'tools'].map(c => check(`model-cap-${c}`, { vision: '视觉', reasoning: '推理', tools: '工具' }[c], m.caps.includes(c))).join('')}</div>`)}<div id="model-efforts" ${m.caps.includes('reasoning') ? '' : 'hidden'}>${field('推理档位', `<div class="efforts">${['关闭', '最低', '低', '中', '高', '极高', '最高'].map(t => `<button type="button" class="effort ${t === '中' ? 'selected' : ''}" aria-pressed="${t === '中'}">${t}</button>`).join('')}</div>`)}</div><p class="metadata">保存后从下一条消息开始生效；正在生成的回复不受影响。</p>`, '保存');
}
function pickerDialog() {
    openModal('挑选模型 · 静态示例（非网络结果）', 640, '<p class="metadata">设计预览未接Backend：未检测线路，未获取模型。下方仅演示原版字段，不代表可用模型。</p><p class="metadata">点 + 添加模型，点 − 移除。</p><div class="row"><input id="picker-search" placeholder="搜索模型 id" aria-label="搜索模型 id"><button type="button" data-write="全部添加">全部添加</button></div><div class="picker-tabs">' + ['全部', ...types].map(t => `<button type="button" data-picker-type="${t}">${t} ${t === '全部' || t === '对话' ? 1 : 0}</button>`).join('') + '</div><div id="picker-example"><div class="group-heading">example <span class="metadata">1</span></div><div class="row"><span class="code grow">example-chat</span><select aria-label="示例模型类型">' + types.map(t => `<option>${t}</option>`).join('') + '</select><span class="metadata">可看图</span><span class="metadata">已选</span><button type="button" data-write="移除模型" aria-label="移除模型">−</button></div></div><p class="metadata">已选 1 个 · 仅展示样式</p>', '', true);
}
$('#provider-search').addEventListener('input', renderRail);
$('#provider-rows').addEventListener('click', event => {
    const selection = event.target.closest('[data-select]');
    const menu = event.target.closest('[data-provider-menu]');
    if (selection) { selected = providers.find(p => p.id === selection.dataset.select); line = selected.lines[0]; folded.clear(); renderRail(); renderDetail(); }
    if (menu) {
        const provider = providers.find(p => p.id === menu.dataset.providerMenu);
        showMenu(menu, [{ label: '编辑', run: () => nameDialog(provider) }, { label: '删除', disabled: provider.preset, run: () => openModal('删除服务商', 440, `<p>删除 ${esc(provider.name)}？其模型将从模型选择器中移除，使用过它们的聊天会保留历史记录。</p>`, '删除') }]);
    }
});
$('#providers-filter').onclick = event => showMenu(event.currentTarget, [['all', '全部'], ['enabled', '启用'], ['disabled', '停用']].map(([value, label]) => ({ label, checked: filter === value, run: () => { filter = value; renderRail(); } })));
$('#providers-add').onclick = () => nameDialog();
$('#provider-detail').addEventListener('input', event => {
    if (event.target.id === 'model-search') renderModels();
});
$('#provider-detail').addEventListener('change', event => {
    if (event.target.id === 'models-cap') renderModels();
    if (event.target.id === 'provider-line') { line = event.target.value; folded.clear(); renderDetail(); }
});
$('#provider-detail').addEventListener('focusout', event => {
    if (['provider-url', 'provider-first-key'].includes(event.target.id)) {
        notice(event.target.id === 'provider-url' ? 'API 地址保存' : '添加密钥');
        if (event.target.type === 'password') event.target.value = '';
    }
});
$('#provider-detail').addEventListener('keydown', event => {
    if (event.key === 'Enter' && ['provider-url', 'provider-first-key'].includes(event.target.id)) { event.preventDefault(); event.target.blur(); }
});
$('#provider-detail').addEventListener('click', event => {
    const target = event.target.closest('button');
    if (!target) return;
    const id = target.id;
    if (id === 'provider-settings') nameDialog(selected);
    if (id === 'provider-keys' || id === 'provider-key-edit') keysDialog(id === 'provider-key-edit');
    if (id === 'provider-line-settings' || id === 'provider-add-line') lineDialog(id === 'provider-line-settings');
    if (id === 'provider-check' || id === 'provider-get-key') notice(id === 'provider-check' ? '检测' : '打开外部密钥获取页面');
    if (id === 'provider-enabled') { selected.enabled = !selected.enabled; target.setAttribute('aria-checked', selected.enabled); renderRail(); notice('启停状态保存（开关仅本页演示）'); }
    if (id === 'models-add') openModal('添加模型', 420, field('模型 ID', input('add-model-id', '', '模型 ID')));
    if (id === 'models-sync') { pickerDialog(); notice('同步模型'); }
    if (id === 'models-fold') { const all = visibleGroups.every(g => folded.has(g)); visibleGroups.forEach(g => all ? folded.delete(g) : folded.add(g)); renderModels(); }
    if (target.dataset.fold) { const group = target.dataset.fold; folded.has(group) ? folded.delete(group) : folded.add(group); renderModels(); }
    if (target.dataset.modelSettings) modelDialog(target.dataset.modelSettings);
    if (target.dataset.write) notice(target.dataset.write);
});
$('#modal-form').onsubmit = event => {
    event.preventDefault(); notice(`${$('#modal-title').textContent}提交`);
    if ($('#keys-value')) $('#keys-value').value = '';
};
$('#modal-form').addEventListener('click', event => {
    const target = event.target.closest('button');
    if (!target) return;
    if (target.hasAttribute('data-close') || target.id === 'modal-close') modal.close();
    if (target.dataset.access) { $('#provider-access').querySelectorAll('button').forEach(b => { b.classList.toggle('selected', b === target); b.setAttribute('aria-pressed', b === target); }); }
    if (target.id === 'line-header-add') { const row = document.createElement('div'); row.className = 'row'; row.innerHTML = '<input placeholder="请求头名称" aria-label="请求头名称"><input placeholder="请求头取值" aria-label="请求头取值"><button class="small-icon" type="button" data-remove-header aria-label="移除请求头">' + icon('minus') + '</button>'; $('#header-fields').append(row); }
    if (target.hasAttribute('data-remove-header')) target.parentElement.remove();
    if (target.id === 'line-default') { target.textContent = '默认（仅草稿）'; notice('设为默认线路'); }
    if (target.id === 'line-remove') openModal('删除线路', 440, `<p>删除 ${short[line]} 线路？只在这条线路上的模型会一起删除。</p>`, '删除');
    if (target.hasAttribute('data-key-edit')) { $('#keys-label').value = selected.keys[Number(target.dataset.keyEdit)]; $('#keys-value').value = ''; $('#keys-submit').textContent = '保存'; $('#keys-keep').hidden = false; $('#keys-value').focus(); }
    if (target.id === 'keys-submit') { notice('密钥写入'); $('#keys-value').value = ''; }
    if (target.id.startsWith('key-enabled-')) { target.setAttribute('aria-checked', target.getAttribute('aria-checked') !== 'true'); notice('密钥启停保存（仅预览）'); }
    if (target.classList.contains('effort')) { const on = target.classList.toggle('selected'); target.setAttribute('aria-pressed', on); }
    if (target.dataset.write) notice(target.dataset.write);
    if (target.dataset.pickerType) $('#picker-example').hidden = !['全部', '对话'].includes(target.dataset.pickerType);
});
$('#modal-form').addEventListener('change', event => {
    if (event.target.id === 'model-cap-reasoning') $('#model-efforts').hidden = !event.target.checked;
    if (event.target.id === 'line-protocol') requestPreview();
});
$('#modal-form').addEventListener('input', event => {
    if (event.target.id === 'line-url') requestPreview();
    if (event.target.id === 'picker-search') $('#picker-example').hidden = !'example-chat'.includes(event.target.value.trim().toLowerCase());
});
modal.addEventListener('close', () => { $('#modal-body').replaceChildren(); });
$('#settings-nav').addEventListener('click', event => { if (event.target.closest('[data-nav]')?.dataset.nav !== 'providers') notice('其他设置页导航（本独立预览仅包含模型服务）'); });
$('#settings-close').onclick = () => { $('#settings-dialog').hidden = true; $('#settings-reopen').hidden = false; };
$('#settings-reopen').onclick = () => { $('#settings-dialog').hidden = false; $('#settings-reopen').hidden = true; };
document.addEventListener('click', event => { if (!event.target.closest('#popup-menu, #providers-filter, [data-provider-menu]')) closeMenu(); });
document.addEventListener('keydown', event => { if (event.key === 'Escape') closeMenu(); });
const source = await fetch('./theme-tokens.json').then(response => { if (!response.ok) throw new Error('无法读取主题令牌'); return response.json(); }).catch(error => { $('#preview-status').textContent = `主题加载失败：${error.message}。请通过本地 HTTP 服务器打开预览。`; return null; });
function applyTheme() {
    if (!source) return;
    const appearance = $('#appearance').value;
    Object.entries(webProperties(source, resolveTheme(source, appearance, $('#accent').value))).forEach(([key, value]) => document.documentElement.style.setProperty(key, value));
    document.documentElement.style.colorScheme = appearance;
}
$('#appearance').onchange = applyTheme;
$('#accent').onchange = applyTheme;
$('#fixture').onchange = event => {
    if (event.target.value === 'populated') populatedFixture();
    else {
        providers.forEach(p => Object.assign(p, { enabled: false, keys: [], lines: ['chat'], models: [] }));
        selected = providers[0]; line = 'chat'; folded.clear(); renderRail(); renderDetail();
    }
};
applyTheme(); icons(); renderRail(); renderDetail();
