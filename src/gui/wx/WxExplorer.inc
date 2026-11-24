 // Included in Frame. A populated directory is not discarded when collapsed.
 fs::path renderedRoot;
 void populate(wxTreeItemId id){
    if(!id.IsOk())return;
    auto* d=static_cast<PathData*>(tree->GetItemData(id));
    if(!d||d->loaded||!wxui::explorer::canExpand(d->path,root))return;
    auto listing=wxui::explorer::list(d->path,root,showHidden);
    tree->DeleteChildren(id);
    for(const auto& entry:listing.entries){
        auto item=tree->AppendItem(id,P(entry.path.filename()),entry.directory?0:1,entry.directory?0:1,new PathData(entry.path));
        tree->SetItemHasChildren(item,entry.expandable);
    }
    if(listing.error){
        tree->AppendItem(id,W("[Cannot read directory: "+listing.error.message()+"]"));
        SetStatusText(W("Explorer: "+d->path.u8string()+": "+listing.error.message()));
    }
    d->loaded=true;
    tree->SetItemHasChildren(id,!listing.entries.empty()||bool(listing.error));
 }
 wxTreeItemId treeFind(const fs::path& p){
    auto node=tree->GetRootItem();if(!node.IsOk()||!wxui::explorer::within(p,root))return {};
    if(p==root)return node;
    for(const auto& part:p.lexically_relative(root)){
        populate(node);
        wxTreeItemIdValue cookie;auto child=tree->GetFirstChild(node,cookie);
        for(;child.IsOk();child=tree->GetNextChild(node,cookie)){
            auto* data=static_cast<PathData*>(tree->GetItemData(child));
            if(data&&data->path.filename()==part)break;
        }
        if(!child.IsOk())return {};
        tree->Expand(node);node=child;
    }
    return node;
 }
 void refreshTree(bool preserve=true){
    std::set<fs::path> expanded;fs::path selection=root;
    if(preserve&&renderedRoot==root){
        auto id=tree->GetSelection();if(id.IsOk())if(auto* d=static_cast<PathData*>(tree->GetItemData(id)))selection=d->path;
        std::function<void(wxTreeItemId)> capture=[&](wxTreeItemId item){
            if(!item.IsOk())return;
            auto* d=static_cast<PathData*>(tree->GetItemData(item));
            if(d&&tree->IsExpanded(item))expanded.insert(d->path);
            wxTreeItemIdValue cookie;
            for(auto c=tree->GetFirstChild(item,cookie);c.IsOk();c=tree->GetNextChild(item,cookie))capture(c);
        };capture(tree->GetRootItem());
    }
    tree->Freeze();
    tree->DeleteAllItems();
    auto r=tree->AddRoot(P(root.filename().empty()?root:root.filename()),0,0,new PathData(root));
    // Explicitly enumerate root; Expand() on a leaf does not emit EXPANDING.
    populate(r);tree->Expand(r);renderedRoot=root;
    for(const auto& p:expanded){auto item=treeFind(p);if(item.IsOk()){populate(item);tree->Expand(item);}}
    auto chosen=treeFind(selection);if(!chosen.IsOk())chosen=r;
    tree->SelectItem(chosen);tree->EnsureVisible(chosen);tree->Thaw();
 }
