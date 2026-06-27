const CAT_META = {
  food: { icon: '🍜', label: 'Food', color: '#4f8ef7' },
  transport: { icon: '🚌', label: 'Transport', color: '#f59e0b' },
  clothes: { icon: '👕', label: 'Clothes', color: '#7c3aed' },
  study: { icon: '📚', label: 'Study', color: '#22c55e' },
  other: { icon: '💡', label: 'Other', color: '#6b7280' },
};

let profile = {
  isStudent: true,
  inHostel: true,
  maritalStatus: null,
  hasKids: null,
  hasLoan: false,
  loanAmount: 0,
};

let budget = 0;
let transactions = [];
let categoryChart = null;
let budgetChart = null;

function formatNPR(amount) {
  return 'NPR ' + Math.round(amount).toLocaleString();
}

function getSpent() {
  return transactions.reduce((sum, tx) => sum + tx.amount, 0);
}

function getDaysLeftInMonth() {
  const now = new Date();
  const lastDay = new Date(now.getFullYear(), now.getMonth() + 1, 0).getDate();
  return lastDay - now.getDate();
}

function aggregateItems() {
  const map = {};
  transactions.forEach(tx => {
    const key = tx.category + '::' + tx.name.trim().toLowerCase();
    if (!map[key]) {
      map[key] = {
        name: tx.name.trim(),
        category: tx.category,
        count: 0,
        totalAmount: 0,
        lastUnitPrice: 0,
      };
    }
    map[key].count += 1;
    map[key].totalAmount += tx.amount;
    map[key].lastUnitPrice = tx.amount;
  });
  return Object.values(map).sort((a, b) => b.count - a.count || b.totalAmount - a.totalAmount);
}

function getCategoryTotals() {
  const totals = {};
  transactions.forEach(tx => {
    totals[tx.category] = (totals[tx.category] || 0) + tx.amount;
  });
  return totals;
}

function show(screenId) {
  document.querySelectorAll('.screen').forEach(s => s.classList.remove('active'));
  document.getElementById(screenId).classList.add('active');
}

function goOnboard() {
  resetOnboarding();
  show('screen-onboard');
}

function resetOnboarding() {
  profile = {
    isStudent: true,
    inHostel: true,
    maritalStatus: null,
    hasKids: null,
    hasLoan: false,
    loanAmount: 0,
  };
  selectStudent(true);
  selectHostel(true);
  selectMarital('single');
  selectKids(false);
  selectLoan(false);
  document.getElementById('loan-amount-in').value = '';
  document.getElementById('budget-in').value = '';
  showObPanel('student-q');
}

function showObPanel(panelId) {
  document.querySelectorAll('.ob-panel').forEach(p => p.classList.add('hidden'));
  document.getElementById('ob-' + panelId).classList.remove('hidden');
  updateStepBar(panelId);
}

function getStepFlow() {
  if (profile.isStudent) {
    return ['student-q', 'hostel', 'budget'];
  }
  if (profile.maritalStatus === 'married') {
    return ['student-q', 'marital', 'kids', 'loan', 'budget'];
  }
  return ['student-q', 'marital', 'loan', 'budget'];
}

function updateStepBar(currentPanel) {
  const flow = getStepFlow();
  const idx = flow.indexOf(currentPanel);
  const bar = document.getElementById('step-bar');
  bar.innerHTML = flow.map((_, i) =>
    `<div class="step-dot${i <= idx ? ' done' : ''}"></div>`
  ).join('');
}

function selectStudent(yes) {
  profile.isStudent = yes;
  document.getElementById('c-yes-student').classList.toggle('selected', yes);
  document.getElementById('c-no-student').classList.toggle('selected', !yes);
}

function selectHostel(yes) {
  profile.inHostel = yes;
  document.getElementById('c-hostel-yes').classList.toggle('selected', yes);
  document.getElementById('c-hostel-no').classList.toggle('selected', !yes);
}

function selectMarital(status) {
  profile.maritalStatus = status;
  document.getElementById('c-single').classList.toggle('selected', status === 'single');
  document.getElementById('c-married').classList.toggle('selected', status === 'married');
}

function selectKids(yes) {
  profile.hasKids = yes;
  document.getElementById('c-kids-yes').classList.toggle('selected', yes);
  document.getElementById('c-kids-no').classList.toggle('selected', !yes);
}

function selectLoan(yes) {
  profile.hasLoan = yes;
  document.getElementById('c-loan-yes').classList.toggle('selected', yes);
  document.getElementById('c-loan-no').classList.toggle('selected', !yes);
  document.getElementById('loan-amount-field').classList.toggle('hidden', !yes);
}

function handleObBack(target) {
  if (target === 'student-q') showObPanel('student-q');
  else if (target === 'marital') showObPanel('marital');
  else if (target === 'loan-back') {
    if (profile.maritalStatus === 'married') showObPanel('kids');
    else showObPanel('marital');
  } else if (target === 'budget-back') {
    if (profile.isStudent) showObPanel('hostel');
    else showObPanel('loan');
  }
}

function goDash() {
  budget = parseInt(document.getElementById('budget-in').value, 10) || 0;
  if (profile.hasLoan) {
    profile.loanAmount = parseInt(document.getElementById('loan-amount-in').value, 10) || 0;
  }

  transactions = [];

  const email = document.getElementById('email-in').value.trim();
  const displayName = email ? email.split('@')[0] : 'User';
  const capitalized = displayName.charAt(0).toUpperCase() + displayName.slice(1);

  document.getElementById('user-name').textContent = capitalized;
  document.getElementById('user-avatar').textContent = capitalized.charAt(0);

  const now = new Date();
  document.getElementById('month-label').textContent =
    now.toLocaleString('default', { month: 'long' }).toUpperCase() + ' ' + now.getFullYear();

  const loanCard = document.getElementById('loan-card');
  if (profile.hasLoan && profile.loanAmount > 0) {
    loanCard.classList.remove('hidden');
    document.getElementById('disp-loan').textContent = formatNPR(profile.loanAmount);
    document.getElementById('loan-note').textContent = 'Outstanding balance';
  } else {
    loanCard.classList.add('hidden');
  }

  refreshDashboard();
  show('screen-dash');
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

  updateInsightText(spent, remaining, pct);
}

function updateInsightText(spent, remaining, pct) {
  const el = document.getElementById('insight-text');
  const month = document.getElementById('month-label').textContent;

  if (transactions.length === 0) {
    el.textContent = `Welcome! Your ${month} tracker starts at zero. Add your first expense to see charts and insights.`;
    return;
  }

  const items = aggregateItems();
  const topItem = items[0];
  const catTotals = getCategoryTotals();
  const topCatKey = Object.keys(catTotals).sort((a, b) => catTotals[b] - catTotals[a])[0];
  const topCat = CAT_META[topCatKey];

  let text = `This month you've spent ${formatNPR(spent)}`;
  if (budget > 0) {
    text += ` (${pct}% of your ${formatNPR(budget)} budget). `;
    text += remaining > 0
      ? `You still have ${formatNPR(remaining)} left with ${getDaysLeftInMonth()} days to go. `
      : `You've exceeded your budget by ${formatNPR(spent - budget)}. `;
  } else {
    text += '. ';
  }

  if (topCat) {
    text += `Your biggest category is ${topCat.label} at ${formatNPR(catTotals[topCatKey])}. `;
  }
  if (topItem) {
    const avg = Math.round(topItem.totalAmount / topItem.count);
    text += `Most bought: "${topItem.name}" — ${topItem.count}× at ~${formatNPR(avg)} each.`;
  }

  if (profile.hasLoan && profile.loanAmount > 0) {
    text += ` You also have an outstanding loan of ${formatNPR(profile.loanAmount)}.`;
  }

  el.textContent = text;
}

function updateHighlights() {
  const items = aggregateItems();
  const catTotals = getCategoryTotals();
  const topCatKey = Object.keys(catTotals).sort((a, b) => catTotals[b] - catTotals[a])[0];

  const topCatEl = document.getElementById('top-category');
  const topCatAmtEl = document.getElementById('top-category-amt');
  const topItemEl = document.getElementById('top-item');
  const topItemDetailEl = document.getElementById('top-item-detail');

  if (topCatKey) {
    const meta = CAT_META[topCatKey];
    topCatEl.textContent = meta.icon + ' ' + meta.label;
    topCatAmtEl.textContent = formatNPR(catTotals[topCatKey]) + ' total spent';
  } else {
    topCatEl.textContent = '—';
    topCatAmtEl.textContent = 'Add expenses to see';
  }

  if (items.length > 0) {
    const top = items[0];
    const avg = Math.round(top.totalAmount / top.count);
    topItemEl.textContent = top.name;
    topItemDetailEl.textContent =
      `Purchased ${top.count}× · Last price ${formatNPR(top.lastUnitPrice)} · Avg ${formatNPR(avg)}`;
  } else {
    topItemEl.textContent = '—';
    topItemDetailEl.textContent = 'Add expenses to see';
  }
}

function renderTopItems() {
  const list = document.getElementById('top-items-list');
  const items = aggregateItems();

  if (items.length === 0) {
    list.innerHTML = '<div class="empty-state">No items yet — add your first expense below!</div>';
    return;
  }

  list.innerHTML = items.map((item, i) => {
    const meta = CAT_META[item.category];
    const avg = Math.round(item.totalAmount / item.count);
    const rankClass = i < 3 ? ` rank-${i + 1}` : '';
    return `
      <div class="top-item${rankClass}">
        <div class="top-item-left">
          <div class="top-rank">${i + 1}</div>
          <div>
            <div class="top-item-name">${meta.icon} ${item.name}</div>
            <div class="top-item-meta">${meta.label} · ${item.count} purchase${item.count > 1 ? 's' : ''}</div>
          </div>
        </div>
        <div class="top-item-right">
          <div class="top-item-price">${formatNPR(item.lastUnitPrice)}<span>/item</span></div>
          <div class="top-item-total">${formatNPR(item.totalAmount)} total · avg ${formatNPR(avg)}</div>
        </div>
      </div>`;
  }).join('');
}

function renderCategoryCards() {
  const grid = document.getElementById('cat-grid');
  const spent = getSpent();
  const items = aggregateItems();

  grid.innerHTML = Object.entries(CAT_META).map(([key, meta]) => {
    const catItems = items.filter(e => e.category === key);
    const total = catItems.reduce((sum, e) => sum + e.totalAmount, 0);
    const pct = spent > 0 ? Math.min(100, Math.round((total / spent) * 100)) : 0;

    const subItems = catItems
      .map(e => {
        const avg = Math.round(e.totalAmount / e.count);
        return `
          <div class="sub-item">
            <span class="sub-name">${e.name}</span>
            <div class="sub-right">
              <span class="sub-count">×${e.count}</span>
              <span class="sub-unit">${formatNPR(e.lastUnitPrice)}/ea</span>
              <span class="sub-amt">${formatNPR(e.totalAmount)}</span>
            </div>
          </div>`;
      })
      .join('');

    return `
      <div class="cat-card">
        <div class="cat-header">
          <div class="cat-name"><span class="cat-icon">${meta.icon}</span>${meta.label}</div>
          <div class="cat-total">${formatNPR(total)}</div>
        </div>
        <div class="progress-bar">
          <div class="progress-fill" style="width:${pct}%;background:linear-gradient(90deg,${meta.color},var(--accent2))"></div>
        </div>
        ${subItems || '<div class="sub-item"><span class="sub-name">No expenses yet</span></div>'}
      </div>`;
  }).join('');
}

function renderRecentList() {
  const list = document.getElementById('recent-list');

  if (transactions.length === 0) {
    list.innerHTML = '<div class="empty-state">No transactions yet.</div>';
    return;
  }

  const sorted = [...transactions].reverse();
  list.innerHTML = sorted.slice(0, 10).map(tx => {
    const meta = CAT_META[tx.category];
    const time = new Date(tx.timestamp).toLocaleString([], {
      month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit',
    });
    return `
      <div class="recent-item">
        <div class="r-left">
          <div class="r-dot" style="background:${meta.color}"></div>
          <div>
            <div class="r-info">${tx.name}</div>
            <div class="r-sub">${meta.icon} ${meta.label} · ${time}</div>
          </div>
        </div>
        <div class="r-amt">-${formatNPR(tx.amount)}</div>
      </div>`;
  }).join('');
}

function renderCharts() {
  const catTotals = getCategoryTotals();
  const spent = getSpent();
  const labels = Object.keys(catTotals).map(k => CAT_META[k].label);
  const values = Object.values(catTotals);
  const colors = Object.keys(catTotals).map(k => CAT_META[k].color);

  const catCtx = document.getElementById('categoryChart').getContext('2d');
  if (categoryChart) categoryChart.destroy();

  if (values.length === 0) {
    document.getElementById('category-caption').textContent = 'No spending recorded yet.';
    categoryChart = new Chart(catCtx, {
      type: 'doughnut',
      data: { labels: ['No data'], datasets: [{ data: [1], backgroundColor: ['#2a3045'] }] },
      options: { plugins: { legend: { display: false } } },
    });
  } else {
    document.getElementById('category-caption').textContent =
      `Total ${formatNPR(spent)} across ${labels.length} categor${labels.length > 1 ? 'ies' : 'y'}.`;
    categoryChart = new Chart(catCtx, {
      type: 'doughnut',
      data: { labels, datasets: [{ data: values, backgroundColor: colors, borderWidth: 0 }] },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
          legend: { position: 'bottom', labels: { color: '#e8eaf0', padding: 14, font: { family: 'Sora', size: 11 } } },
        },
        cutout: '65%',
      },
    });
  }

  const budCtx = document.getElementById('budgetChart').getContext('2d');
  if (budgetChart) budgetChart.destroy();

  const remaining = Math.max(0, budget - spent);
  const overBudget = spent > budget && budget > 0;

  document.getElementById('budget-caption').textContent = budget > 0
    ? (overBudget
      ? `Over budget by ${formatNPR(spent - budget)}!`
      : `${formatNPR(remaining)} remaining of ${formatNPR(budget)} budget.`)
    : 'Set a budget during onboarding to track limits.';

  budgetChart = new Chart(budCtx, {
    type: 'bar',
    data: {
      labels: ['Spent', budget > 0 ? 'Remaining' : 'Budget not set'],
      datasets: [{
        data: budget > 0 ? [spent, remaining] : [spent, 0],
        backgroundColor: [overBudget ? '#ef4444' : '#4f8ef7', '#22c55e'],
        borderRadius: 8,
        borderSkipped: false,
      }],
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: { legend: { display: false } },
      scales: {
        x: { ticks: { color: '#6b7280', font: { family: 'Sora' } }, grid: { display: false } },
        y: {
          ticks: {
            color: '#6b7280',
            font: { family: 'JetBrains Mono', size: 10 },
            callback: v => 'NPR ' + v.toLocaleString(),
          },
          grid: { color: '#2a3045' },
          beginAtZero: true,
        },
      },
    },
  });
}

function refreshDashboard() {
  updateSummary();
  updateHighlights();
  renderTopItems();
  renderCategoryCards();
  renderRecentList();
  renderCharts();
}

function addExpense() {
  const name = document.getElementById('exp-name').value.trim();
  const amt = parseInt(document.getElementById('exp-amt').value, 10);
  const cat = document.getElementById('exp-cat').value;

  if (!name || !amt || amt <= 0) {
    showToast('Enter a valid item name and price', true);
    return;
  }

  transactions.push({
    name,
    amount: amt,
    category: cat,
    timestamp: Date.now(),
  });

  document.getElementById('exp-name').value = '';
  document.getElementById('exp-amt').value = '';

  refreshDashboard();
  showToast('✓ Expense added!');
}

function showToast(msg, isError) {
  const toast = document.getElementById('toast');
  toast.textContent = msg;
  toast.style.background = isError ? '#ef4444' : '#22c55e';
  toast.classList.add('show');
  setTimeout(() => toast.classList.remove('show'), 2200);
}

function setupEventListeners() {
  document.getElementById('btn-sign-in').addEventListener('click', goOnboard);
  document.getElementById('btn-create-account').addEventListener('click', goOnboard);

  document.getElementById('c-yes-student').addEventListener('click', () => selectStudent(true));
  document.getElementById('c-no-student').addEventListener('click', () => selectStudent(false));
  document.getElementById('c-hostel-yes').addEventListener('click', () => selectHostel(true));
  document.getElementById('c-hostel-no').addEventListener('click', () => selectHostel(false));
  document.getElementById('c-single').addEventListener('click', () => selectMarital('single'));
  document.getElementById('c-married').addEventListener('click', () => selectMarital('married'));
  document.getElementById('c-kids-yes').addEventListener('click', () => selectKids(true));
  document.getElementById('c-kids-no').addEventListener('click', () => selectKids(false));
  document.getElementById('c-loan-yes').addEventListener('click', () => selectLoan(true));
  document.getElementById('c-loan-no').addEventListener('click', () => selectLoan(false));

  document.getElementById('ob-next-student-q').addEventListener('click', () => {
    showObPanel(profile.isStudent ? 'hostel' : 'marital');
  });
  document.getElementById('ob-next-hostel').addEventListener('click', () => showObPanel('budget'));
  document.getElementById('ob-next-marital').addEventListener('click', () => {
    showObPanel(profile.maritalStatus === 'married' ? 'kids' : 'loan');
  });
  document.getElementById('ob-next-kids').addEventListener('click', () => showObPanel('loan'));
  document.getElementById('ob-next-loan').addEventListener('click', () => showObPanel('budget'));

  document.querySelectorAll('.ob-back').forEach(btn => {
    btn.addEventListener('click', () => handleObBack(btn.dataset.back));
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

document.addEventListener('DOMContentLoaded', () => {
  setupEventListeners();
  resetOnboarding();
});
