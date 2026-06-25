const CAT_META = {
  food: { icon: '🍜', label: 'Food', color: '#4f8ef7' },
  transport: { icon: '🚌', label: 'Transport', color: '#f59e0b' },
  clothes: { icon: '👕', label: 'Clothes', color: '#7c3aed' },
  study: { icon: '📚', label: 'Study', color: '#22c55e' },
  other: { icon: '💡', label: 'Other', color: '#6b7280' },
};

const SAMPLE_EXPENSES = [
  { name: 'Momo', amount: 980, category: 'food', count: 7 },
  { name: 'Dal Bhat', amount: 1800, category: 'food', count: 12 },
  { name: 'Snacks / Tea', amount: 720, category: 'food', count: 18 },
  { name: 'Chowmein', amount: 700, category: 'food', count: 5 },
  { name: 'Bus (local)', amount: 880, category: 'transport', count: 22 },
  { name: 'Taxi / Ride', amount: 970, category: 'transport', count: 6 },
  { name: 'T-shirt', amount: 900, category: 'clothes', count: 2 },
  { name: 'Socks / Inner', amount: 400, category: 'clothes', count: 3 },
  { name: 'Slippers', amount: 300, category: 'clothes', count: 1 },
  { name: 'Printouts', amount: 300, category: 'study', count: 30 },
  { name: 'Stationery', amount: 250, category: 'study', count: 5 },
  { name: 'Lab Materials', amount: 250, category: 'study', count: 2 },
];

const SAMPLE_RECENT = [
  { name: 'Momo at canteen', category: 'food', amount: 140, time: 'Today, 1:15 PM' },
  { name: 'Bus to Dhulikhel', category: 'transport', amount: 80, time: 'Today, 9:00 AM' },
  { name: 'Dal Bhat dinner', category: 'food', amount: 150, time: 'Yesterday, 7:30 PM' },
  { name: 'A4 Printouts (30 pages)', category: 'study', amount: 90, time: 'Yesterday, 2:10 PM' },
];

let userType = 'student';
let currentStep = 1;
let budget = 15000;
let expenses = [...SAMPLE_EXPENSES];
let recentTransactions = [...SAMPLE_RECENT];

function formatNPR(amount) {
  return 'NPR ' + amount.toLocaleString();
}

function getSpent() {
  return expenses.reduce((sum, e) => sum + e.amount, 0);
}

function getDaysLeftInMonth() {
  const now = new Date();
  const lastDay = new Date(now.getFullYear(), now.getMonth() + 1, 0).getDate();
  return lastDay - now.getDate();
}

function show(screenId) {
  document.querySelectorAll('.screen').forEach(s => s.classList.remove('active'));
  document.getElementById(screenId).classList.add('active');
}

function goOnboard() {
  show('screen-onboard');
}

function selectChoice(type) {
  userType = type;
  document.getElementById('c-student').classList.toggle('selected', type === 'student');
  document.getElementById('c-other').classList.toggle('selected', type === 'other');
}

function selectHostel(value) {
  document.getElementById('c-hostel').classList.toggle('selected', value === 'yes');
  document.getElementById('c-home').classList.toggle('selected', value === 'no');
}

function toggleSel(el) {
  el.classList.toggle('selected');
}

function nextStep(n) {
  currentStep = n;
  ['ob-step1', 'ob-step2-student', 'ob-step2-other', 'ob-step3'].forEach(id => {
    document.getElementById(id).classList.add('hidden');
  });
  document.querySelectorAll('.step-dot').forEach((dot, i) => {
    dot.classList.toggle('done', i < n);
  });

  if (n === 1) {
    document.getElementById('ob-step1').classList.remove('hidden');
  } else if (n === 2) {
    const stepId = userType === 'student' ? 'ob-step2-student' : 'ob-step2-other';
    document.getElementById(stepId).classList.remove('hidden');
  } else if (n === 3) {
    document.getElementById('ob-step3').classList.remove('hidden');
  }
}

function updateSummary() {
  const spent = getSpent();
  const remaining = Math.max(0, budget - spent);
  const pct = budget > 0 ? Math.round((spent / budget) * 100) : 0;

  document.getElementById('disp-budget').textContent = formatNPR(budget);
  document.getElementById('disp-spent').textContent = formatNPR(spent);
  document.getElementById('disp-left').textContent = formatNPR(remaining);
  document.getElementById('spent-note').textContent = `${pct}% of budget used`;
  document.getElementById('remaining-note').textContent =
    `${getDaysLeftInMonth()} days left in month`;
}

function renderCategoryCards() {
  const grid = document.getElementById('cat-grid');
  const spent = getSpent();

  grid.innerHTML = Object.entries(CAT_META).map(([key, meta]) => {
    const items = expenses.filter(e => e.category === key);
    const total = items.reduce((sum, e) => sum + e.amount, 0);
    const pct = spent > 0 ? Math.min(100, Math.round((total / spent) * 100)) : 0;

    const subItems = items
      .map(e => `
        <div class="sub-item">
          <span class="sub-name">${e.name}</span>
          <div class="sub-right">
            <span class="sub-count">×${e.count}</span>
            <span class="sub-amt">${formatNPR(e.amount)}</span>
          </div>
        </div>`)
      .join('');

    return `
      <div class="cat-card">
        <div class="cat-header">
          <div class="cat-name"><span class="cat-icon">${meta.icon}</span>${meta.label}</div>
          <div class="cat-total">${formatNPR(total)}</div>
        </div>
        <div class="progress-bar">
          <div class="progress-fill" style="width:${pct}%"></div>
        </div>
        ${subItems || '<div class="sub-item"><span class="sub-name">No expenses yet</span></div>'}
      </div>`;
  }).join('');
}

function renderRecentList() {
  const list = document.getElementById('recent-list');
  list.innerHTML = recentTransactions.map(tx => {
    const meta = CAT_META[tx.category];
    return `
      <div class="recent-item">
        <div class="r-left">
          <div class="r-dot" style="background:${meta.color}"></div>
          <div>
            <div class="r-info">${tx.name}</div>
            <div class="r-sub">${meta.icon} ${meta.label} · ${tx.time}</div>
          </div>
        </div>
        <div class="r-amt">-${formatNPR(tx.amount)}</div>
      </div>`;
  }).join('');
}

function refreshDashboard() {
  updateSummary();
  renderCategoryCards();
  renderRecentList();
}

function goDash() {
  budget = parseInt(document.getElementById('budget-in').value, 10) || 15000;

  const email = document.getElementById('email-in').value.trim();
  const displayName = email ? email.split('@')[0] : 'Gaurav';
  const initial = displayName.charAt(0).toUpperCase();

  document.getElementById('user-name').textContent =
    displayName.charAt(0).toUpperCase() + displayName.slice(1);
  document.getElementById('user-avatar').textContent = initial;

  const now = new Date();
  document.getElementById('month-label').textContent =
    now.toLocaleString('default', { month: 'long' }).toUpperCase() + ' ' + now.getFullYear();

  refreshDashboard();
  show('screen-dash');
}

function addExpense() {
  const name = document.getElementById('exp-name').value.trim();
  const amt = parseInt(document.getElementById('exp-amt').value, 10);
  const cat = document.getElementById('exp-cat').value;

  if (!name || !amt) return;

  const existing = expenses.find(e => e.category === cat && e.name === name);
  if (existing) {
    existing.amount += amt;
    existing.count += 1;
  } else {
    expenses.push({ name, amount: amt, category: cat, count: 1 });
  }

  const now = new Date();
  const timeStr = 'Today, ' + now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
  recentTransactions.unshift({ name, category: cat, amount: amt, time: timeStr });

  document.getElementById('exp-name').value = '';
  document.getElementById('exp-amt').value = '';

  refreshDashboard();

  const toast = document.getElementById('toast');
  toast.classList.add('show');
  setTimeout(() => toast.classList.remove('show'), 2000);
}

function setupEventListeners() {
  document.getElementById('btn-sign-in').addEventListener('click', goOnboard);
  document.getElementById('btn-create-account').addEventListener('click', goOnboard);

  document.getElementById('c-student').addEventListener('click', () => selectChoice('student'));
  document.getElementById('c-other').addEventListener('click', () => selectChoice('other'));
  document.getElementById('c-hostel').addEventListener('click', () => selectHostel('yes'));
  document.getElementById('c-home').addEventListener('click', () => selectHostel('no'));

  document.querySelectorAll('[data-toggle-sel]').forEach(btn => {
    btn.addEventListener('click', () => toggleSel(btn));
  });

  document.querySelectorAll('[data-step]').forEach(btn => {
    btn.addEventListener('click', () => nextStep(parseInt(btn.dataset.step, 10)));
  });

  document.getElementById('btn-start-tracking').addEventListener('click', goDash);
  document.getElementById('btn-add-expense').addEventListener('click', addExpense);

  document.getElementById('exp-name').addEventListener('keydown', e => {
    if (e.key === 'Enter') addExpense();
  });
  document.getElementById('exp-amt').addEventListener('keydown', e => {
    if (e.key === 'Enter') addExpense();
  });
}

document.addEventListener('DOMContentLoaded', setupEventListeners);
