<script>
  import { onMount } from 'svelte';
  import { page } from '$app/stores';
  import { auth } from '$lib/stores';
  import { api } from '$lib/api';
  import { goto } from '$app/navigation';

  const classId = $page.params.id;
  let results = [], classData = null, loading = true, error = '';
  let expanded = {};

  onMount(async () => {
    if (!$auth.isAuthenticated) { goto('/'); return; }
    try {
      [classData, results] = await Promise.all([
        api.getClass(classId),
        api.getClassResults(classId)
      ]);
    } catch (e) {
      error = e.message;
    } finally {
      loading = false;
    }
  });

  function toggle(id) {
    expanded = { ...expanded, [id]: !expanded[id] };
  }

  function formatDate(d) {
    if (!d) return '';
    return new Date(d).toLocaleString('ko-KR', {
      month: 'short', day: 'numeric',
      hour: '2-digit', minute: '2-digit'
    });
  }

  function scoreColor(s) {
    if (s >= 8.5) return 'text-emerald-600';
    if (s >= 7) return 'text-blue-600';
    if (s >= 5) return 'text-yellow-600';
    return 'text-red-500';
  }

  function scoreBg(s) {
    if (s >= 8.5) return 'bg-emerald-50 border-emerald-200';
    if (s >= 7) return 'bg-blue-50 border-blue-200';
    if (s >= 5) return 'bg-yellow-50 border-yellow-200';
    return 'bg-red-50 border-red-200';
  }

  $: totalPresentations = results.length;
  $: totalScores = results.reduce((a, p) => a + p.score_count, 0);
  $: classAvg = results.length > 0
    ? (results.filter(p => p.average_score !== null).reduce((a, p) => a + (p.average_score || 0), 0) /
       results.filter(p => p.average_score !== null).length).toFixed(1)
    : null;
</script>

<svelte:head><title>평가 결과 - {classData?.name || ''}</title></svelte:head>

<div class="max-w-4xl mx-auto px-4 py-8">
  <div class="flex items-center gap-3 mb-6">
    <a href="/teacher/class/{classId}" class="text-blue-600 hover:text-blue-800 text-sm font-medium">← 반 관리</a>
    <span class="text-gray-300">/</span>
    <span class="text-gray-500 text-sm">{classData?.name || ''}</span>
  </div>

  <div class="mb-6">
    <h1 class="text-2xl font-bold text-gray-800">📊 평가 결과</h1>
    <p class="text-gray-400 text-sm mt-1">교사만 볼 수 있는 발표 평가 결과입니다</p>
  </div>

  {#if error}
    <div class="bg-red-50 border border-red-200 text-red-600 px-4 py-3 rounded-xl mb-4 text-sm">{error}</div>
  {/if}

  <!-- Summary stats -->
  {#if !loading && results.length > 0}
    <div class="grid grid-cols-3 gap-4 mb-6">
      <div class="card text-center">
        <div class="text-3xl font-bold text-blue-600">{totalPresentations}</div>
        <div class="text-sm text-gray-400 mt-1">발표 횟수</div>
      </div>
      <div class="card text-center">
        <div class="text-3xl font-bold text-blue-600">{totalScores}</div>
        <div class="text-sm text-gray-400 mt-1">전체 평가 수</div>
      </div>
      <div class="card text-center">
        {#if classAvg}
          <div class="text-3xl font-bold {scoreColor(parseFloat(classAvg))}">{classAvg}</div>
        {:else}
          <div class="text-3xl font-bold text-gray-300">-</div>
        {/if}
        <div class="text-sm text-gray-400 mt-1">반 평균 점수</div>
      </div>
    </div>
  {/if}

  {#if loading}
    <div class="text-center py-16 text-gray-400">로딩 중...</div>
  {:else if results.length === 0}
    <div class="text-center py-20 bg-white rounded-2xl border-2 border-dashed border-gray-200">
      <div class="text-5xl mb-3">📝</div>
      <p class="text-gray-400">아직 발표 기록이 없습니다</p>
    </div>
  {:else}
    <div class="space-y-3">
      {#each results as p}
        <div class="bg-white rounded-2xl shadow-sm border border-gray-100 overflow-hidden">
          <!-- Presentation header -->
          <button
            class="w-full px-6 py-4 flex items-center justify-between hover:bg-gray-50 transition-colors text-left"
            on:click={() => toggle(p.id)}
          >
            <div class="flex items-center gap-4">
              <div class="w-11 h-11 rounded-full bg-blue-100 flex items-center justify-center text-blue-700 font-bold text-lg shrink-0">
                {p.presenter?.name?.[0] || '?'}
              </div>
              <div>
                <div class="font-bold text-gray-800">{p.presenter?.name}</div>
                {#if p.title}
                  <div class="text-sm text-gray-500 mt-0.5">"{p.title}"</div>
                {/if}
                <div class="text-xs text-gray-300 mt-0.5">{formatDate(p.created_at)}</div>
              </div>
            </div>

            <div class="flex items-center gap-4 shrink-0">
              {#if p.is_active}
                <span class="text-xs bg-green-100 text-green-700 px-2 py-1 rounded-full font-semibold">진행중</span>
              {/if}
              <div class="text-right">
                {#if p.average_score !== null && p.average_score !== undefined}
                  <div class="text-2xl font-bold {scoreColor(p.average_score)}">
                    {p.average_score.toFixed(1)}
                  </div>
                  <div class="text-xs text-gray-300">/ 10점</div>
                {:else}
                  <div class="text-gray-300 text-sm">평가 없음</div>
                {/if}
              </div>
              <div class="text-center">
                <div class="text-xl font-bold text-gray-600">{p.score_count}</div>
                <div class="text-xs text-gray-300">평가자</div>
              </div>
              <span class="text-gray-300 text-lg">{expanded[p.id] ? '▲' : '▼'}</span>
            </div>
          </button>

          <!-- Expanded scores -->
          {#if expanded[p.id]}
            <div class="border-t border-gray-100 px-6 py-4">
              {#if p.scores.length === 0}
                <p class="text-gray-400 text-sm text-center py-4">평가 내용이 없습니다</p>
              {:else}
                <div class="space-y-3">
                  {#each p.scores as s}
                    <div class="flex items-start gap-3 bg-gray-50 rounded-xl p-3">
                      <div class="w-9 h-9 rounded-full bg-gray-200 flex items-center justify-center text-gray-600 font-bold text-sm shrink-0">
                        {s.evaluator?.name?.[0] || '?'}
                      </div>
                      <div class="flex-1 min-w-0">
                        <div class="text-sm font-semibold text-gray-700">
                          {s.evaluator?.name || '익명'}
                          {#if s.evaluator?.student_number}
                            <span class="text-gray-400 font-normal"> ({s.evaluator.student_number}번)</span>
                          {/if}
                        </div>
                        {#if s.comment}
                          <div class="text-sm text-gray-600 mt-1 bg-white rounded-lg px-3 py-2 border border-gray-100">
                            "{s.comment}"
                          </div>
                        {:else}
                          <div class="text-xs text-gray-300 mt-1">코멘트 없음</div>
                        {/if}
                      </div>
                      <div class="shrink-0 text-right">
                        <div class="text-xl font-bold {scoreColor(s.score)}">{s.score}</div>
                        <div class="text-xs text-gray-300">/ 10</div>
                      </div>
                    </div>
                  {/each}
                </div>
              {/if}
            </div>
          {/if}
        </div>
      {/each}
    </div>
  {/if}
</div>
