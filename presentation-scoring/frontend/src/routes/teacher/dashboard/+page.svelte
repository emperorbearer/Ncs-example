<script>
  import { onMount } from 'svelte';
  import { auth } from '$lib/stores';
  import { api } from '$lib/api';
  import { goto } from '$app/navigation';

  let classes = [], loading = true, error = '';
  let showModal = false, cname = '', cdesc = '', creating = false;

  onMount(async () => {
    if (!$auth.isAuthenticated) { goto('/'); return; }
    await load();
  });

  async function load() {
    loading = true;
    try { classes = await api.getClasses(); }
    catch (e) { error = e.message; }
    finally { loading = false; }
  }

  async function create() {
    if (!cname.trim()) return;
    creating = true;
    try {
      const c = await api.createClass({ name: cname.trim(), description: cdesc.trim() || null });
      classes = [...classes, { ...c, student_count: 0 }];
      showModal = false; cname = ''; cdesc = '';
    } catch (e) { error = e.message; }
    finally { creating = false; }
  }

  async function del(id, name) {
    if (!confirm(`"${name}" 반을 삭제하시겠습니까?\n(학생 명단과 모든 평가 데이터가 삭제됩니다)`)) return;
    try {
      await api.deleteClass(id);
      classes = classes.filter((c) => c.id !== id);
    } catch (e) { error = e.message; }
  }

  function formatDate(d) {
    return new Date(d).toLocaleDateString('ko-KR', { year: 'numeric', month: 'short', day: 'numeric' });
  }
</script>

<svelte:head><title>대시보드 - 발표 평가 시스템</title></svelte:head>

<div class="max-w-5xl mx-auto px-4 py-8">
  <div class="flex justify-between items-start mb-8">
    <div>
      <h1 class="text-2xl font-bold text-gray-800">내 반 관리</h1>
      <p class="text-gray-400 text-sm mt-1">반을 만들고 학생 발표를 관리하세요</p>
    </div>
    <button
      on:click={() => (showModal = true)}
      class="bg-blue-600 hover:bg-blue-700 text-white px-5 py-2.5 rounded-xl font-medium flex items-center gap-2 transition-colors"
    >
      <span class="text-lg leading-none">+</span> 새 반 만들기
    </button>
  </div>

  {#if error}
    <div class="bg-red-50 border border-red-200 text-red-600 px-4 py-3 rounded-xl mb-4 text-sm">
      {error}
    </div>
  {/if}

  {#if loading}
    <div class="text-center py-16 text-gray-400">로딩 중...</div>
  {:else if classes.length === 0}
    <div class="text-center py-20 bg-white rounded-2xl border-2 border-dashed border-gray-200">
      <div class="text-6xl mb-3">📚</div>
      <p class="text-gray-500 font-semibold text-lg mb-1">아직 반이 없습니다</p>
      <p class="text-gray-400 text-sm mb-5">새 반을 만들어 학생 발표를 시작하세요</p>
      <button
        on:click={() => (showModal = true)}
        class="bg-blue-600 hover:bg-blue-700 text-white px-8 py-3 rounded-xl font-semibold transition-colors"
      >반 만들기</button>
    </div>
  {:else}
    <div class="grid gap-4 sm:grid-cols-2 lg:grid-cols-3">
      {#each classes as c}
        <div class="bg-white rounded-2xl shadow-sm border border-gray-100 p-6 hover:shadow-md transition-shadow">
          <div class="flex justify-between items-start mb-2">
            <h2 class="font-bold text-lg text-gray-800 leading-tight">{c.name}</h2>
            <button
              on:click={() => del(c.id, c.name)}
              class="text-gray-200 hover:text-red-400 text-2xl leading-none ml-2 transition-colors"
              title="삭제"
            >×</button>
          </div>
          {#if c.description}
            <p class="text-gray-400 text-sm mb-3">{c.description}</p>
          {/if}
          <div class="flex items-center gap-3 text-sm text-gray-400 mb-4">
            <span>👥 학생 {c.student_count}명</span>
            <span>•</span>
            <span>{formatDate(c.created_at)}</span>
          </div>
          <div class="flex gap-2">
            <a
              href="/teacher/class/{c.id}"
              class="flex-1 text-center bg-blue-600 hover:bg-blue-700 text-white py-2.5 rounded-xl text-sm font-semibold transition-colors"
            >발표 관리</a>
            <a
              href="/teacher/results/{c.id}"
              class="flex-1 text-center bg-emerald-600 hover:bg-emerald-700 text-white py-2.5 rounded-xl text-sm font-semibold transition-colors"
            >결과 보기</a>
          </div>
        </div>
      {/each}
    </div>
  {/if}
</div>

{#if showModal}
  <div
    class="fixed inset-0 bg-black/50 flex items-center justify-center z-50 p-4"
    on:click|self={() => (showModal = false)}
  >
    <div class="bg-white rounded-2xl shadow-2xl w-full max-w-md p-6">
      <h2 class="text-xl font-bold text-gray-800 mb-5">새 반 만들기</h2>
      <div class="space-y-4">
        <div>
          <label class="block text-sm font-medium text-gray-700 mb-1">반 이름 <span class="text-red-500">*</span></label>
          <input
            type="text" bind:value={cname} placeholder="예: 3학년 1반"
            class="input" autofocus
          />
        </div>
        <div>
          <label class="block text-sm font-medium text-gray-700 mb-1">설명 (선택)</label>
          <input
            type="text" bind:value={cdesc} placeholder="예: 2024년 소프트웨어 개발 수업"
            class="input"
          />
        </div>
        <div class="flex gap-3 pt-2">
          <button
            on:click={() => { showModal = false; cname = ''; cdesc = ''; }}
            class="btn-secondary flex-1"
          >취소</button>
          <button
            on:click={create}
            disabled={creating || !cname.trim()}
            class="btn-primary flex-1"
          >{creating ? '생성 중...' : '만들기'}</button>
        </div>
      </div>
    </div>
  </div>
{/if}
