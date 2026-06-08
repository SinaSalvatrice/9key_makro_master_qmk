package com.ninekey.configurator.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.EditText
import android.widget.TextView
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import com.ninekey.configurator.R
import com.ninekey.configurator.model.KeyAssignment
import com.ninekey.configurator.model.KeycodeCategory
import com.ninekey.configurator.model.KeycodeEntry
import com.ninekey.configurator.model.KeycodeCatalog

/**
 * Bottom sheet dialog for editing a single key assignment.
 *
 * Shows:
 * - Current key info (position, label, description)
 * - Category spinner to filter the keycode list
 * - A scrollable list of keycodes in the selected category
 * - A hex input field for advanced users
 *
 * Calls [onKeyAssigned] when the user picks a new keycode.
 */
class KeyEditorSheet : BottomSheetDialogFragment() {

    var catalog: KeycodeCatalog? = null
    var layerId: Int = 0
    var keyIndex: Int = 0
    var currentAssignment: KeyAssignment = KeyAssignment()
    var onKeyAssigned: ((KeyAssignment) -> Unit)? = null

    private var selectedEntry: KeycodeEntry? = null

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View = inflater.inflate(R.layout.bottom_sheet_key_editor, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val cat = catalog ?: return

        // ── Current key info ──────────────────────────────────────────────────
        view.findViewById<TextView>(R.id.keyEditorTitle).text =
            "Key ${keyIndex + 1} — ${currentAssignment.label}"
        view.findViewById<TextView>(R.id.keyEditorDescription).text =
            currentAssignment.description.ifBlank { "No description" }
        view.findViewById<TextView>(R.id.keyEditorCurrentCode).text =
            "Current: 0x%04X → ${currentAssignment.label}".format(currentAssignment.code)

        // ── OLED label edit ───────────────────────────────────────────────────
        val oledInput = view.findViewById<EditText>(R.id.keyEditorOledInput)
        oledInput.setText(currentAssignment.oledLabel.take(5))

        // ── Category spinner ──────────────────────────────────────────────────
        val categories = cat.getCategories()
        val categoryNames = categories.map { it.label }
        val spinner = view.findViewById<android.widget.Spinner>(R.id.keyEditorCategorySpinner)
        val spinnerAdapter =
            ArrayAdapter(requireContext(), android.R.layout.simple_spinner_item, categoryNames)
        spinnerAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        spinner.adapter = spinnerAdapter

        // ── Keycode list ──────────────────────────────────────────────────────
        val recycler = view.findViewById<RecyclerView>(R.id.keyEditorRecycler)
        recycler.layoutManager = LinearLayoutManager(requireContext())

        var currentCategory = categories.firstOrNull() ?: return
        val adapter = KeycodeAdapter(currentCategory.keycodes) { entry ->
            selectedEntry = entry
            applyEntry(view, entry, oledInput, cat)
        }
        recycler.adapter = adapter

        spinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(
                parent: AdapterView<*>?, v: View?, position: Int, id: Long
            ) {
                currentCategory = categories[position]
                adapter.updateList(currentCategory.keycodes)
            }
            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }

        // ── Hex input ─────────────────────────────────────────────────────────
        val hexInput = view.findViewById<EditText>(R.id.keyEditorHexInput)
        hexInput.setText("0x%04X".format(currentAssignment.code))

        view.findViewById<View>(R.id.keyEditorHexApply).setOnClickListener {
            val parsed = parseHex(hexInput.text.toString())
            if (parsed != null) {
                val label = cat.displayLabel(parsed)
                val entry = KeycodeEntry(parsed, label, label, cat.descriptionFor(parsed), "manual")
                applyEntry(view, entry, oledInput, cat)
            }
        }

        // ── Apply / Cancel ────────────────────────────────────────────────────
        view.findViewById<View>(R.id.keyEditorApplyButton).setOnClickListener {
            val entry = selectedEntry ?: return@setOnClickListener
            val oledText = oledInput.text.toString().trim().take(5)
            onKeyAssigned?.invoke(
                KeyAssignment(
                    code = entry.code,
                    label = cat.displayLabel(entry.code),
                    oledLabel = oledText.ifBlank { cat.displayLabel(entry.code).take(5) },
                    description = entry.description
                )
            )
            dismiss()
        }

        view.findViewById<View>(R.id.keyEditorCancelButton).setOnClickListener { dismiss() }
    }

    private fun applyEntry(
        view: View, entry: KeycodeEntry, oledInput: EditText, cat: KeycodeCatalog
    ) {
        view.findViewById<TextView>(R.id.keyEditorSelectedLabel).text =
            "${entry.display}  (${entry.label})"
        view.findViewById<TextView>(R.id.keyEditorSelectedDesc).text =
            entry.description.ifBlank { cat.descriptionFor(entry.code) }
        view.findViewById<EditText>(R.id.keyEditorHexInput).setText(
            "0x%04X".format(entry.code)
        )
        if (oledInput.text.isBlank()) {
            oledInput.setText(entry.display.take(5))
        }
    }

    private fun parseHex(text: String): Int? {
        val t = text.trim()
        return if (t.startsWith("0x", ignoreCase = true)) {
            t.substring(2).toIntOrNull(16)?.takeIf { it in 0..0xFFFF }
        } else {
            t.toIntOrNull()?.takeIf { it in 0..0xFFFF }
        }
    }

    companion object {
        fun newInstance(
            layerId: Int,
            keyIndex: Int,
            current: KeyAssignment,
            catalog: KeycodeCatalog,
            callback: (KeyAssignment) -> Unit
        ): KeyEditorSheet {
            return KeyEditorSheet().also {
                it.layerId = layerId
                it.keyIndex = keyIndex
                it.currentAssignment = current
                it.catalog = catalog
                it.onKeyAssigned = callback
            }
        }
    }
}

// ── RecyclerView adapter for keycode list ─────────────────────────────────────

private class KeycodeAdapter(
    private var items: List<KeycodeEntry>,
    private val onClick: (KeycodeEntry) -> Unit
) : RecyclerView.Adapter<KeycodeAdapter.VH>() {

    private var selectedPos = RecyclerView.NO_ID.toInt()

    inner class VH(itemView: View) : RecyclerView.ViewHolder(itemView) {
        val display: TextView = itemView.findViewById(R.id.keycodeDisplay)
        val label: TextView = itemView.findViewById(R.id.keycodeLabel)
        val desc: TextView = itemView.findViewById(R.id.keycodeDesc)
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VH {
        val v = LayoutInflater.from(parent.context)
            .inflate(R.layout.item_keycode_entry, parent, false)
        return VH(v)
    }

    override fun onBindViewHolder(holder: VH, position: Int) {
        val entry = items[position]
        holder.display.text = entry.display
        holder.label.text = entry.label
        holder.desc.text = entry.description
        holder.itemView.isSelected = position == selectedPos
        holder.itemView.setOnClickListener {
            val prev = selectedPos
            selectedPos = holder.bindingAdapterPosition
            notifyItemChanged(prev)
            notifyItemChanged(selectedPos)
            onClick(entry)
        }
    }

    override fun getItemCount() = items.size

    fun updateList(newItems: List<KeycodeEntry>) {
        items = newItems
        selectedPos = RecyclerView.NO_ID.toInt()
        notifyDataSetChanged()
    }
}
